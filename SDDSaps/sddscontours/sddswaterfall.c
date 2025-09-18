/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * SDDSWATERFALL - 3D Waterfall Plot Generator with Movie Export
 * 
 * DESCRIPTION:
 *   Creates interactive 3D waterfall plots from SDDS data files. The program
 *   reads X, Y, Z data columns for a single page file or reads the independent
 *   column (X data), spectra data (Z column) and Y data in the parameter for multiple page file
 *   and generates either an interactive OpenGL visualization or exports to gnuplot format. 
 *   Supports both waterfall curve display and 3D surface contour modes. Now includes movie 
 *   export capability for rotation animations.
 * 
 * FEATURES:
 *   - Interactive 3D visualization with mouse controls
 *   - Multiple display modes: waterfall curves and 3D surface contours
 *   - Gnuplot export capability for external plotting
 *   - Customizable viewing angles and zoom levels
 *   - Professional color schemes and axis labeling with improved single-character label visibility
 *   - PNG screenshot export with high-quality rendering
 *   - Movie export for rotation animations (MP4, GIF, or frame sequences)
 *   - Enhanced axis label rendering with automatic scaling for optimal readability
 *   - Support for both single-page and multi-page SDDS data formats
 *   - Automatic background process management for non-blocking operation
 * 
 * RECENT IMPROVEMENTS:
 *   - Fixed single-character axis label visibility issues for small data ranges
 *   - Enhanced label scaling algorithm to ensure readability regardless of data scale
 *   - Improved stroke text rendering with minimum size constraints
 *   - Added comprehensive debug output for troubleshooting label rendering
 * 
 * COMPILATION:
 *   Requires: OpenGL, GLFW, GLUT, SDDS libraries
 *   gcc -o sddswaterfall sddswaterfall.c -lglfw -lGL -lGLU -lglut -lSDDS1 -lmdbcommon -lm
 * 
 * AUTHOR: SDDS Development Team
 * VERSION: Enhanced with improved axis label rendering (2024)
 * 
 */

#include <GL/glut.h>  //include GLUT first
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDDS.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>     // for fork(), setsid()
#include <sys/types.h>  // for pid_t
#include <sys/stat.h>   // for mkdir()
#include "scan.h"
#include "mdb.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define SET_SINGLEPAGE 0
#define SET_MULTIPAGE 1
#define SET_OUTPUT 2
#define SET_SURFACE 3
#define SET_GNUPLOTSURFACE 4
#define SET_VIEWANGLE 5
#define SET_ZOOM 6
#define SET_ROTATEVIEW 7
#define SET_MOVIEEXPORT 8
#define SET_SWAPXY 9
#define SET_CMAP 10
#define SET_XSCALE 11
#define SET_YSCALE 12
#define SET_XLABEL 13
#define SET_YLABEL 14
#define SET_ZLABEL 15
#define SET_LABELSCALE 16

#define OPTIONS 17
static char *option[OPTIONS]={
  "singlePage", "multiPage", "outputFile", "surface", "gnuplotSurface", "viewAngle", "zoom", "rotateView", "movieExport", "swapxy", "cmap", "xscale", "yscale", "xlabel", "ylabel", "zlabel", "labelScale" };

// Movie export parameters
typedef struct {
    bool enabled;           // whether to export movie
    char format[16];        // "mp4", "gif", or "frames"
    char filename[256];     // output movie filename
    int fps;               // frames per second
    int width;             // movie width
    int height;            // movie height
    char temp_dir[256];    // temporary directory for frames
} MovieParams;

// Rotation animation parameters
typedef struct {
  char axis;           // 'x', 'y', or 'z'
  double min_angle;    // minimum rotation angle
  double max_angle;    // maximum rotation angle
  int positions;       // number of positions in rotation
  double pause;        // pause time in seconds between positions
  bool enabled;        // whether rotation is enabled
  int current_pos;     // current position in animation
  double last_time;    // last time position was updated
  int direction;       // 1 for forward, -1 for backward
  bool save_frames;    // whether to save animation frames
  int frame_count;     // current frame number for saving
  bool movie_mode;     // true when recording movie
  int total_frames;    // total frames for complete rotation cycle
} RotationParams;

RotationParams rotation = {0};
MovieParams movie = {0};

/*for multipage data, need provide xcol, ycol and zparam, x,y,z becomes xcol, ycol, and zparam
  while ycol will be the color column (z-axis) and zparam will be plotted a y-axis */
typedef struct {
  double *x_data;     // X column data for this page
  double *y_data;     // Color column data for this page for multiplage or ycol data for single page
  double *z_data;    // zcol data for single page.
  double z_param;     // Z parameter value for this page for multipage data
  int n_points;       // Number of points in this page
} PageData;

typedef struct {
    double x, y, z;
    float r, g, b;      // Color values
} ScatterPoint;

// Color maps
typedef enum {
    CMAP_JET = 0,
    CMAP_COOLWARM = 1,
    CMAP_VIRIDIS = 2,
    CMAP_PLASMA = 3
} ColorMap;

// Global variables
PageData *pages = NULL;
int n_pages = 0;
ScatterPoint *scatter_points = NULL;
int n_scatter_points = 0;
double yscale_min = 0, yscale_max = 0;
double xscale_min = 0, xscale_max = 0;
bool yscale_set = false;
bool xscale_set = false;
bool swap_xy = false;
bool multi_page=false;
bool single_page=false;
ColorMap color_map = CMAP_JET;
/*
 * USAGE INFORMATION
 */
char *USAGE = "\
sddswaterfall creates interactive 3D waterfall plots from SDDS data files. The program \n\
reads X, Y, Z data columns for a single page file or reads the independent column (X data), \n\
spectra data (Z column) and Y data in the parameter for multiple page file. \n\
and generates either an interactive OpenGL visualization or exports to gnuplot format. \n\
Supports both waterfall curve display and 3D surface contour modes. Now includes movie \n\
export capability for rotation animations.\n\
\n\
sddswaterfall [<inputfile>] [<options>]\n\
\n\
REQUIRED OPTIONS:\n\
  -singlePage=<xcol>,<ycol>,<zcol>  Specify column names for X, Y, and Z data for the single-page data \n\
or \n\
  -multiPage=<xcol>,<zcol>,<parameter>  for multi-page data, the x and z data are provided by the xcol and zcol, the y data is provided by the parameter.\n\
\n\
either -singlePage or -multiPage need to be provided, but can not be provided at the same time. \n\
OPTIONAL PARAMETERS:\n\
  -outputFile=<filename>     Output PNG filename (default: screenshot.png)\n\
  -surface                   Enable 3D surface contour mode instead of waterfall curves\n\
  -gnuplotSurface           Export data to gnuplot for external plotting\n\
  -viewAngle=x=<angle>,y=<angle>,z=<angle>  Set initial viewing angles in degrees\n\
                            (default: x=-75, y=0, z=-45)\n\
  -zoom=<value>             Set initial zoom level (default: -5.0)\n\
  -rotateView=axis=<axis>,min=<angle>,max=<angle>,positions=<n>,pause=<seconds>\n\
                            Enable automatic rotation animation around specified axis\n\
                            axis: x, y, or z\n\
                            min/max: rotation range in degrees\n\
                            positions: number of animation steps\n\
                            pause: seconds between steps\n\
                            Example: -rotateView=axis=z,min=-45,max=45,positions=9,pause=2\n\
  -movieExport=format=<format>,filename=<name>,fps=<rate>,width=<w>,height=<h>\n\
                            Export rotation animation as movie\n\
                            format: mp4, gif, or frames (default: mp4)\n\
                            filename: output movie file (default: rotation_movie)\n\
                            fps: frames per second (default: 10)\n\
                            width/height: movie dimensions (default: 1200x900)\n\
                            Example: -movieExport=format=mp4,filename=my_rotation,fps=15\n\
  -yscale=<min>,<max>       Sets the y-axis limits to a specific range\n\
  -xscale=<min>,<max>       Sets the x-axis limits to a specific range\n\
  -xlabel=<string>          Sets a custom label for the x-axis (overrides column name)\n\
  -ylabel=<string>          Sets a custom label for the y-axis (overrides column name)\n\
  -zlabel=<string>          Sets a custom label for the z-axis (overrides column name)\n\
  -cmap=<colormap>          'jet', 'coolwarm', 'viridis', or 'plasma' (default: jet)\n\
  -swapxy                   swap x and y axis in the plot.\n\
\n\
INTERACTIVE CONTROLS:\n\
  Mouse drag:               Rotate the 3D view (disabled during movie recording)\n\
  Up/Down arrow keys:       Zoom in/out\n\
  ESC key:                  Stop rotation animation\n\
  Close window:             Save screenshot and exit\n\
\n\
DISPLAY MODES:\n\
  Default (waterfall):      Shows colored curves in 3D space with matplotlib-style colors\n\
  -surface:                 Shows 3D surface with contour lines and color mapping\n\
  -gnuplotSurface:          Exports data to gnuplot (no interactive display)\n\
\n\
MOVIE EXPORT:\n\
  When -movieExport is used with -rotateView, the program will:\n\
  1. Record frames during the rotation animation\n\
  2. Convert frames to the specified movie format\n\
  3. Clean up temporary files\n\
  \n\
  Supported formats:\n\
  - mp4: H.264 encoded video (requires ffmpeg)\n\
  - gif: Animated GIF (requires ffmpeg or imagemagick)\n\
  - frames: Keep individual PNG frames in a directory\n\
\n\
EXAMPLES:\n\
  # Basic 3D plot\n\
  sddswaterfall waterfall3d.sdds -singlePage=x,y,z\n\
\n\
 # surface 3D plot\n\
  sddswaterfall waterfall3d.sdds -singlePage=x,y,z -surface\n\
\n\
#gnuplot \n\
  sddswaterfall waterfall3d.sdds -singlePage=x,y,z -gnu \n\
\n\
  #multiplage example \n\
  sddswaterfall S-LFB:Z:SRAM:SPEC-01.gz  -multipage=S-LFB:Z:SRAM:FREQ,S-LFB:Z:SRAM:SPEC,S-DCCT:CurrentM \n\
  \n\
  #with cmap option \n\
  sddswaterfall S-LFB:Z:SRAM:SPEC-01.gz  -multipage=S-LFB:Z:SRAM:FREQ,S-LFB:Z:SRAM:SPEC,S-DCCT:CurrentM -surface -cmap=plasma \n\
\n\
  # Create rotation animation and export as MP4\n\
  sddswaterfall waterfall3d.sdds -singlePage=x,y,z -rotateView=axis=z,min=-45,max=45,positions=36,pause=0.1 -movieExport=format=mp4,filename=rotation_z_axis,fps=10\n\
  \n\
  # Create GIF animation\n\
  sddswaterfall waterfall3d.sdds -singlePage=x,y,z -rotateView=axis=y,min=-90,max=90,positions=60,pause=0.05 -movieExport=format=gif,filename=rotation_y_axis,fps=20\n\
  \n\
  # Save individual frames only\n\
  sddswaterfall waterfall3d.sdds -singelPage=x,y,z -rotateView=axis=x,min=0,max=360,positions=72,pause=0.1 -movieExport=format=frames,filename=rotation_frames\n\
\n\
DATA REQUIREMENTS:\n\
  - Input file must be in SDDS format\n\
  - Must contain at least three numeric columns for X, Y, Z data\n\
  - Data points will be interpolated onto a regular grid for visualization\n\
\n\
OUTPUT:\n\
  - Interactive OpenGL window (except with -gnuplotSurface)\n\
  - PNG screenshot saved on exit\n\
  - Movie file (MP4/GIF) or frame directory when using -movieExport\n\
  - Gnuplot commands and data (with -gnuplotSurface option)\n\
\n\
DEPENDENCIES:\n\
  - SDDS library for data input\n\
  - OpenGL, GLFW, GLUT for 3D graphics\n\
  - ffmpeg (for MP4/GIF export)\n\
  - Optional: imagemagick (alternative for GIF export)\n\
  - Optional: gnuplot for external plotting\n\
";

// Function declarations
void print_usage();
void format_tick(float val, char* buf, size_t buflen);
void format_tick_improved(float val, char* base_buf, char* exp_buf, size_t buflen);
void get_palette_color(float t, float* r, float* g, float* b);
void generate_gnuplot_surface();
void draw_colorbar(float xMin, float xMax, float yMin, float yMax, float zMin, float zMax);
void save_frame(const char* filename, int width, int height);
void create_movie_from_frames();
void cleanup_temp_files();
void get_color_from_map(double value, double min_val, double max_val, ColorMap cmap, float *r, float *g, float *b);
void load_sdds_data(char *filename, char *xcol, char *ycol, char *zcol);
void prepare_multi_page_grid_data();
/*
 * Print usage information
 */
void print_usage() {
    fprintf(stderr, "%s", USAGE);
}

#define NUM_CURVES 50
#define POINTS_PER_CURVE 100

float zdata[NUM_CURVES][POINTS_PER_CURVE];
float xgrid[POINTS_PER_CURVE], ygrid[NUM_CURVES];
double angleX = -75.0f, angleY = 0.0f, angleZ = -45.0f, zoomZ = -5.0f;

char xlabel[128] = "X";
char ylabel[128] = "Y";
char zlabel[128] = "Z";

// Label scaling factors
double xlabel_scale = 1.0;
double ylabel_scale = 1.0;
double zlabel_scale = 1.0;

bool surface_mode = false;  // Flag for 3D surface contour mode
bool gnuplot_surface = false;  // Flag for gnuplot surface mode

struct Sample {
    float x, y, z;
};

typedef struct Sample Sample;

float distance_squared(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

float inverse_distance_interpolate(float x, float y, Sample* samples, int n) {
    float sum = 0.0f;
    float weight_sum = 0.0f;
    const float epsilon = 1e-12f;
    for (int i = 0; i < n; i++) {
        float d2 = distance_squared(x, y, samples[i].x, samples[i].y);
        if (d2 < epsilon) return samples[i].z;
        float weight = 1.0f / (d2 + epsilon);
        sum += samples[i].z * weight;
        weight_sum += weight;
    }
    return (weight_sum > 0.0f) ? sum / weight_sum : 0.0f;
}

void interpolate_grid(Sample* samples, int nsamples, float* xgrid, int nx, float* ygrid, int ny, float Z[NUM_CURVES][POINTS_PER_CURVE]) {
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            Z[j][i] = inverse_distance_interpolate(xgrid[i], ygrid[j], samples, nsamples);
        }
    }
}

// Improved function to format numbers with separate exponential display
void format_tick_improved(float val, char* base_buf, char* exp_buf, size_t buflen) {
    if (fabs(val) < 1e-3 || fabs(val) > 1e4) {
        int exp = (int)floor(log10(fabs(val)));
        float base = val / pow(10, exp);
        snprintf(base_buf, buflen, "%.2f", base);
        snprintf(exp_buf, buflen, "e%d", exp);
    } else {
        snprintf(base_buf, buflen, "%.2f", val);
        exp_buf[0] = '\0';  // Empty exponential part
    }
}

// Color palette function similar to gnuplot (yellow-red-purple-black) - kept for backward compatibility
void get_palette_color(float t, float* r, float* g, float* b) {
    // Clamp t to [0,1] range
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    // Gnuplot-like palette: high values = yellow, low values = purple/black
    if (t > 0.75f) {
        // Yellow to red
        float local_t = (t - 0.75f) * 4.0f;
        *r = 1.0f;
        *g = 1.0f - local_t * 0.5f;
        *b = 0.0f;
    } else if (t > 0.5f) {
        // Red to purple
        float local_t = (t - 0.5f) * 4.0f;
        *r = 1.0f;
        *g = 0.5f - local_t * 0.5f;
        *b = local_t * 0.5f;
    } else if (t > 0.25f) {
        // Purple to dark purple
        float local_t = (t - 0.25f) * 4.0f;
        *r = 1.0f - local_t * 0.5f;
        *g = 0.0f;
        *b = 0.5f;
    } else {
        // Dark purple to black
        float local_t = t * 4.0f;
        *r = 0.5f - local_t * 0.4f;
        *g = 0.0f;
        *b = 0.5f - local_t * 0.3f;
    }
}

// Unified color mapping function that supports different color maps
void get_color_from_map(double value, double min_val, double max_val, ColorMap cmap, float *r, float *g, float *b) {
    // Normalize value to [0,1] range
    float t = 0.0f;
    if (max_val != min_val) {
        t = (float)((value - min_val) / (max_val - min_val));
    }
    
    // Clamp t to [0,1] range
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    switch (cmap) {
        case CMAP_JET:
            // Jet colormap: blue -> cyan -> green -> yellow -> red
            if (t < 0.125f) {
                *r = 0.0f;
                *g = 0.0f;
                *b = 0.5f + 4.0f * t;
            } else if (t < 0.375f) {
                *r = 0.0f;
                *g = 4.0f * (t - 0.125f);
                *b = 1.0f;
            } else if (t < 0.625f) {
                *r = 4.0f * (t - 0.375f);
                *g = 1.0f;
                *b = 1.0f - 4.0f * (t - 0.375f);
            } else if (t < 0.875f) {
                *r = 1.0f;
                *g = 1.0f - 4.0f * (t - 0.625f);
                *b = 0.0f;
            } else {
                *r = 1.0f - 2.0f * (t - 0.875f);
                *g = 0.0f;
                *b = 0.0f;
            }
            break;
            
        case CMAP_COOLWARM:
            // Cool-warm colormap: blue -> white -> red
            if (t < 0.5f) {
                float local_t = 2.0f * t;
                *r = local_t;
                *g = local_t;
                *b = 1.0f;
            } else {
                float local_t = 2.0f * (t - 0.5f);
                *r = 1.0f;
                *g = 1.0f - local_t;
                *b = 1.0f - local_t;
            }
            break;
            
        case CMAP_VIRIDIS:
            // Viridis colormap: dark purple -> blue -> green -> yellow
            if (t < 0.25f) {
                float local_t = 4.0f * t;
                *r = 0.267f * local_t;
                *g = 0.004f + 0.349f * local_t;
                *b = 0.329f + 0.344f * local_t;
            } else if (t < 0.5f) {
                float local_t = 4.0f * (t - 0.25f);
                *r = 0.267f + 0.081f * local_t;
                *g = 0.353f + 0.196f * local_t;
                *b = 0.673f - 0.064f * local_t;
            } else if (t < 0.75f) {
                float local_t = 4.0f * (t - 0.5f);
                *r = 0.348f + 0.478f * local_t;
                *g = 0.549f + 0.302f * local_t;
                *b = 0.609f - 0.475f * local_t;
            } else {
                float local_t = 4.0f * (t - 0.75f);
                *r = 0.826f + 0.167f * local_t;
                *g = 0.851f + 0.145f * local_t;
                *b = 0.134f + 0.866f * local_t;
            }
            break;
            
        case CMAP_PLASMA:
            // Plasma colormap: dark purple -> magenta -> orange -> yellow
            if (t < 0.25f) {
                float local_t = 4.0f * t;
                *r = 0.050f + 0.498f * local_t;
                *g = 0.030f + 0.074f * local_t;
                *b = 0.528f + 0.349f * local_t;
            } else if (t < 0.5f) {
                float local_t = 4.0f * (t - 0.25f);
                *r = 0.548f + 0.339f * local_t;
                *g = 0.104f + 0.215f * local_t;
                *b = 0.877f - 0.184f * local_t;
            } else if (t < 0.75f) {
                float local_t = 4.0f * (t - 0.5f);
                *r = 0.887f + 0.100f * local_t;
                *g = 0.319f + 0.434f * local_t;
                *b = 0.693f - 0.549f * local_t;
            } else {
                float local_t = 4.0f * (t - 0.75f);
                *r = 0.987f + 0.013f * local_t;
                *g = 0.753f + 0.247f * local_t;
                *b = 0.144f + 0.856f * local_t;
            }
            break;
            
        default:
            // Fallback to jet colormap
            get_color_from_map(value, min_val, max_val, CMAP_JET, r, g, b);
            break;
    }
}

// Generate gnuplot surface plot (similar to test3dwaterfall1.c)
void generate_gnuplot_surface() {
    FILE *gnuplotPipe = popen("gnuplot -persistent", "w");

    if (gnuplotPipe) {
        fprintf(gnuplotPipe, "set term qt\n");
        fprintf(gnuplotPipe, "set view 60, 30\n");
        fprintf(gnuplotPipe, "unset key\n");
        fprintf(gnuplotPipe, "set ticslevel 0\n");
        fprintf(gnuplotPipe, "set xlabel '%s'\n", xlabel);
        fprintf(gnuplotPipe, "set ylabel '%s'\n", ylabel);
        fprintf(gnuplotPipe, "set zlabel '%s'\n", zlabel);
        fprintf(gnuplotPipe, "splot '-' using 1:2:3 with lines palette lw 2\n");
        
        // Send the interpolated data to gnuplot
        for (int j = 0; j < NUM_CURVES; ++j) {
            for (int i = 0; i < POINTS_PER_CURVE; ++i) {
                fprintf(gnuplotPipe, "%f %f %e\n", xgrid[i], ygrid[j], zdata[j][i]);
            }
            fprintf(gnuplotPipe, "\n");  // Blank line to separate curves
        }
        
        fprintf(gnuplotPipe, "e\n");
        fflush(gnuplotPipe);
        pclose(gnuplotPipe);
        
        printf("Gnuplot surface plot generated successfully.\n");
    } else {
        fprintf(stderr, "Error: Could not open gnuplot pipe. Make sure gnuplot is installed.\n");
    }
}

static float computeStrokeTextWidth(void *font, const char *text) {
    float width = 0.0f;

    if (!font || !text)
        return width;

    for (const unsigned char *c = (const unsigned char *)text; *c; ++c)
        width += glutStrokeWidth(font, *c);

    return width;
}

void draw_axes(float xMin, float xMax, float yMin, float yMax, float zMin, float zMax) {
    // Draw main axis lines in conventional 3D layout (like matplotlib)
    glLineWidth(2.0f);
    glColor3f(0.8f, 0.8f, 0.8f);  // Light gray for main axes visibility against black background

    glBegin(GL_LINES);
    // X axis - along bottom front edge
    glVertex3f(xMin, yMin, zMin);
    glVertex3f(xMax, yMin, zMin);
    
    // Y axis - along bottom right edge  
    glVertex3f(xMax, yMin, zMin);  
    glVertex3f(xMax, yMax, zMin);
    
    // Additional Y axis - along bottom left edge
    glVertex3f(xMin, yMin, zMin);
    glVertex3f(xMin, yMax, zMin);
    
    // Z axis - along left-front edge (MOVED TO LEFT-FRONT)
    glVertex3f(xMin, yMin, zMin);
    glVertex3f(xMin, yMin, zMax);
    glEnd();
    
    glLineWidth(1.0f);

    // Draw X-Y grid system (bottom plane)
    glColor3f(0.8f, 0.8f, 0.8f);  // Light gray for grid
    glLineWidth(0.8f);
    glBegin(GL_LINES);
    
    int ticks = 10;
    
    // Grid lines parallel to X-axis (across Y direction) - bottom plane
    for (int i = 0; i <= ticks; ++i) {
        float y = yMin + i * (yMax - yMin) / ticks;
        glVertex3f(xMin, y, zMin);
        glVertex3f(xMax, y, zMin);
    }
    
    // Grid lines parallel to Y-axis (across X direction) - bottom plane
    for (int i = 0; i <= ticks; ++i) {
        float x = xMin + i * (xMax - xMin) / ticks;
        glVertex3f(x, yMin, zMin);
        glVertex3f(x, yMax, zMin);
    }
    
    glEnd();
    glLineWidth(1.0f);

    // Draw axis labels and tick labels
    glColor3f(1.0f, 1.0f, 1.0f);  // White text for visibility against black background

    // === Axis labels (parallel to axes) ===

    // X-axis label - positioned at center of X-axis, below the axis
    const float kStrokeRomanHeight = 119.05f;
    
    // Dynamic scaling factor based on xlabel length to maintain consistent visual font size
    float kAxisLabelScaleFactor = 0.01;
    
    float x_range = fabs(xMax - xMin);
    float y_range = fabs(yMax - yMin);
    float z_range = fabs(zMax - zMin);
    /* if (x_range < 1e-6f)
        x_range = 1.0f;
    if (y_range < 1e-6f)
        y_range = x_range;
    if (z_range < 1e-6f)
    z_range = x_range; */

    float global_scale_x = 2.0f / x_range;
    float global_scale_y = 2.0f / y_range;
    float global_scale_z = 2.0f / z_range;

    float inv_scale_x = global_scale_x ? 1.0f / global_scale_x : 1.0f;
    float inv_scale_y = global_scale_y ? 1.0f / global_scale_y : 1.0f;
    float inv_scale_z = global_scale_z ? 1.0f / global_scale_z : 1.0f;

    float label_center_x = (xMin + xMax) / 2.0f;
    float baseline_y = yMin - 0.15f * y_range;
    float label_z = zMin;

    float xStrokeWidthUnits = computeStrokeTextWidth(GLUT_STROKE_ROMAN, xlabel);
    if (xStrokeWidthUnits <= 0.0f)
        xStrokeWidthUnits = 1.0f;

    // Set minimum width for single character labels to ensure they're visible
    float min_width_for_single_char = 150.0f;  // Increased minimum stroke width units for single characters
    if (strlen(xlabel) == 1 && xStrokeWidthUnits < min_width_for_single_char) {
        xStrokeWidthUnits = min_width_for_single_char;
    }
    
    float xMaxWidth = 0.35f * x_range;
    float xMaxHeight = 0.12f * y_range;
    
    // For single character labels, ensure minimum readable size regardless of data range
    if (strlen(xlabel) == 1) {
        float min_label_width = 0.05f * x_range;  // Minimum 5% of x_range for single chars
        if (xMaxWidth < min_label_width) {
            xMaxWidth = min_label_width;
        }
    }
    
    float xWidthScale = xMaxWidth / xStrokeWidthUnits;
    float xHeightScale = xMaxHeight / kStrokeRomanHeight;
    float xStrokeScale = xWidthScale < xHeightScale ? xWidthScale : xHeightScale;
    if (xStrokeScale <= 0.0f)
        xStrokeScale = (0.1f * x_range) / xStrokeWidthUnits;
    xStrokeScale *= kAxisLabelScaleFactor*strlen(xlabel);
    
    // Apply user-specified label scaling
    xStrokeScale *= xlabel_scale;
    
    // For single character labels, ensure minimum scale factor
    if (strlen(xlabel) == 1 && xStrokeScale < 0.001f) {
        xStrokeScale = 0.001f;  // Minimum scale factor for single characters
    }
    
    float text_height = xStrokeScale * kStrokeRomanHeight;
    if (baseline_y + text_height > yMin)
        baseline_y = yMin - (text_height + 0.03f * y_range);

    glPushMatrix();
    glTranslatef(label_center_x, baseline_y, label_z);
    glScalef(inv_scale_x, inv_scale_y, inv_scale_z);
    glScalef(xStrokeScale, xStrokeScale, xStrokeScale);
    glTranslatef(-0.5f * xStrokeWidthUnits, 0.0f, 0.0f);
    glLineWidth(1.5f);  // Thicker for better visibility
    for (const char *c = xlabel; *c; c++)
      glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);  // Use proportional font for natural readability
    glLineWidth(1.0f);
    glPopMatrix();

    // Y-axis label - centered and rotated so it tracks the axis direction
    float y_label_center = (yMin + yMax) / 2.0f;
    float y_label_x = xMax + 0.18f * x_range;
    float y_label_z = zMin;

    float yStrokeWidthUnits = computeStrokeTextWidth(GLUT_STROKE_ROMAN, ylabel);
    if (yStrokeWidthUnits <= 0.0f)
        yStrokeWidthUnits = 1.0f;

    // Calculate Y-axis label scaling independently
    float yMaxWidth = 0.35f * y_range;  // Use y_range for Y-axis
    float yMaxHeight = 0.12f * x_range;  // Use x_range for Y-axis height constraint
    
    // For single character labels, ensure minimum readable size
    if (strlen(ylabel) == 1) {
        float min_label_width = 0.05f * y_range;
        if (yMaxWidth < min_label_width) {
            yMaxWidth = min_label_width;
        }
    }
    
    float yWidthScale = yMaxWidth / yStrokeWidthUnits;
    float yHeightScale = yMaxHeight / kStrokeRomanHeight;
    float yStrokeScale = yWidthScale < yHeightScale ? yWidthScale : yHeightScale;
    if (yStrokeScale <= 0.0f)
        yStrokeScale = (0.1f * y_range) / yStrokeWidthUnits;
    yStrokeScale *= kAxisLabelScaleFactor*strlen(ylabel)/4.0;
    
    // Apply user-specified label scaling
    yStrokeScale *= ylabel_scale;
    
    // For single character labels, ensure minimum scale factor
    if (strlen(ylabel) == 1 && yStrokeScale < 0.001f) {
        yStrokeScale = 0.001f;
    }

    float half_length = 0.5f * yStrokeWidthUnits * yStrokeScale * inv_scale_y;
    if (y_label_center - half_length < yMin)
        y_label_center = yMin + half_length + 0.02f * y_range;
    if (y_label_center + half_length > yMax)
        y_label_center = yMax - half_length - 0.02f * y_range;

    float glyph_extent_x = kStrokeRomanHeight * yStrokeScale * inv_scale_x;
    float min_clearance = 0.05f * x_range;
    if (y_label_x - glyph_extent_x < xMax + min_clearance)
        y_label_x = xMax + min_clearance + glyph_extent_x;

    glPushMatrix();
    glTranslatef(y_label_x, y_label_center, y_label_z);
    glScalef(inv_scale_x, inv_scale_y, inv_scale_z);
    glScalef(yStrokeScale, yStrokeScale, yStrokeScale);
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-0.5f * yStrokeWidthUnits, 0.0f, 0.0f);
    glLineWidth(1.5f);  // Thicker for better visibility
    for (const char *c = ylabel; *c; ++c)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);  // Use proportional font for natural readability
    glLineWidth(1.0f);
    glPopMatrix();
    
    // Z-axis label - centered along the axis and rendered with stroke text parallel to Z
    float z_label_center = (zMin + zMax) / 2.0f;
    float z_label_x = xMin - 0.12f * x_range;
    float z_label_y = yMin - 0.10f * y_range;

    float zStrokeWidthUnits = computeStrokeTextWidth(GLUT_STROKE_ROMAN, zlabel);
    if (zStrokeWidthUnits <= 0.0f)
        zStrokeWidthUnits = 1.0f;

    // Calculate Z-axis label scaling independently
    float zMaxWidth = 0.35f * z_range;  // Use z_range for Z-axis
    float zMaxHeight = 0.12f * y_range;  // Use y_range for Z-axis height constraint
    
    // For single character labels, ensure minimum readable size
    if (strlen(zlabel) == 1) {
        float min_label_width = 0.05f * z_range;
        if (zMaxWidth < min_label_width) {
            zMaxWidth = min_label_width;
        }
    }
    
    float zWidthScale = zMaxWidth / zStrokeWidthUnits;
    float zHeightScale = zMaxHeight / kStrokeRomanHeight;
    float zStrokeScale = zWidthScale < zHeightScale ? zWidthScale : zHeightScale;
    if (zStrokeScale <= 0.0f)
        zStrokeScale = (0.1f * z_range) / zStrokeWidthUnits;
    zStrokeScale *= kAxisLabelScaleFactor*strlen(zlabel)/4.0;
    
    // Apply user-specified label scaling
    zStrokeScale *= zlabel_scale;
    
    // For single character labels, ensure minimum scale factor
    if (strlen(zlabel) == 1 && zStrokeScale < 0.001f) {
        zStrokeScale = 0.001f;
    }

    float zGlyphHeight = kStrokeRomanHeight * zStrokeScale * inv_scale_y;
    float y_clearance = 0.03f * y_range;
    float max_y_for_label = yMin - y_clearance;
    if (z_label_y + zGlyphHeight > max_y_for_label)
        z_label_y = max_y_for_label - zGlyphHeight;

    float x_clearance = 0.05f * x_range;
    float min_x_for_label = xMin - x_clearance;
    if (z_label_x > min_x_for_label)
        z_label_x = min_x_for_label;

    glPushMatrix();
    glTranslatef(z_label_x, z_label_y, z_label_center);
    glScalef(inv_scale_x, inv_scale_y, inv_scale_z);
    glScalef(zStrokeScale, zStrokeScale, zStrokeScale);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-0.5f * zStrokeWidthUnits, 0.0f, 0.0f);
    glLineWidth(1.5f);  // Thicker for better visibility
    for (const char *c = zlabel; *c; ++c)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);  // Use proportional font for natural readability
    glLineWidth(1.0f);
    glPopMatrix();
   
 
    // Determine overall X-axis exponent for display
    float abs_max_x = fmax(fabs(xMin), fabs(xMax));
    int x_overall_exponent = 0;
    if (abs_max_x != 0) {
        x_overall_exponent = (int)floor(log10(abs_max_x));
        x_overall_exponent = (x_overall_exponent / 3) * 3; // Engineering notation
    }

    // X tick labels with exponential scaling when needed
    for (int i = 0; i <= ticks; ++i) {
        float frac = i / (float)ticks;
        float x_actual = xMin + frac * (xMax - xMin);
        char label[24];
        
        // Apply the overall scaling factor to the tick values
        float x_display_value = x_actual * powf(10.0f, -x_overall_exponent);
        snprintf(label, sizeof(label), "%.2f", x_display_value);
        
        // Keep exact horizontal positioning, but move further down for better spacing
        glRasterPos3f(x_actual, yMin - 0.08 * (yMax - yMin), zMin);  // Moved further down
        for (char* c = label; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Add dynamic scale indicator for X-axis if exponent is not zero
    if (x_overall_exponent != 0) {
        char x_scale_label_str[32];
        snprintf(x_scale_label_str, sizeof(x_scale_label_str), "x1e%d", x_overall_exponent);
        glRasterPos3f(xMax, yMin - 0.12f * (yMax - yMin), zMin);
        for (const char* c = x_scale_label_str; *c; ++c)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }


    // Y tick labels on right side - moved closer to axis
    int y_ticks = 10;
    for (int i = 0; i <= y_ticks; ++i) {
        float frac = i / (float)y_ticks;
        float y = yMin + frac * (yMax - yMin);
        char label[24];
        snprintf(label, sizeof(label), "%.2f", y);  // 2 decimal places
        
        glRasterPos3f(xMax + 0.03 * (xMax - xMin), y, zMin);  // Moved closer to axis
        for (char* c = label; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Z tick labels at left-front - 10 ticks with proper spacing
    int z_ticks = 10;
    
    // Draw Z-axis tick marks and labels
    glColor3f(0.8f, 0.8f, 0.8f);  // Light gray for tick marks
    glBegin(GL_LINES);
    for (int i = 0; i <= z_ticks; ++i) {
        float frac = i / (float)z_ticks;
        float z_actual = zMin + frac * (zMax - zMin);
        
        // Draw tick mark on Z-axis
        glVertex3f(xMin, yMin, z_actual);
        glVertex3f(xMin - 0.02f * (xMax - xMin), yMin, z_actual);
    }
    glEnd();

    // Determine overall Z-axis exponent for display
    float abs_max_z = fmax(fabs(zMin), fabs(zMax));
    int z_overall_exponent = 0;
    if (abs_max_z != 0) {
        z_overall_exponent = (int)floor(log10(abs_max_z));
        z_overall_exponent = (z_overall_exponent / 3) * 3; // Engineering notation
    }

    // Draw Z-axis tick labels with consistent spacing and 2 decimals
    glColor3f(1.0f, 1.0f, 1.0f);  // White text
    for (int i = 0; i <= z_ticks; ++i) {
        float frac = i / (float)z_ticks;
        float z_actual = zMin + frac * (zMax - zMin);
        
        char label[24];
        // Apply the overall scaling factor to the tick values
        float z_display_value = z_actual * powf(10.0f, -z_overall_exponent);
        snprintf(label, sizeof(label), "%.2f", z_display_value);
        
        // Adjust spacing based on whether the number is negative (needs more space for minus sign)
        float label_offset = (z_display_value < 0) ? 0.18f : 0.15f; // Use z_display_value for offset calculation
        glRasterPos3f(xMin - label_offset * (xMax - xMin), yMin, z_actual);
        for (char* c = label; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Add dynamic scale indicator at the top end of Z-axis if exponent is not zero
    if (z_overall_exponent != 0) {
        char scale_label_str[32];
        snprintf(scale_label_str, sizeof(scale_label_str), "x1e%d", z_overall_exponent);
        glRasterPos3f(xMin - 0.15f * (xMax - xMin), yMin, zMax + 0.08f * (zMax - zMin));
        for (const char* c = scale_label_str; *c; ++c)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
}

void draw_grid(float xMin, float xMax, float yMin, float yMax, float zPlane) {
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINES);
    for (int i = 0; i <= 10; ++i) {
        float x = xMin + i * (xMax - xMin) / 10;
        glVertex3f(x, yMin, zPlane);
        glVertex3f(x, yMax, zPlane);
        float y = yMin + i * (yMax - yMin) / 10;
        glVertex3f(xMin, y, zPlane);
        glVertex3f(xMax, y, zPlane);
    }
    glEnd();
}

void draw_colorbar(float xMin, float xMax, float yMin, float yMax, float zMin, float zMax) {
    // Position colorbar vertically along Z-axis at the right side
    float bar_x = xMax + 0.2f * (xMax - xMin);
    float bar_y = yMax;  // Fixed Y position at back
    float bar_width = 0.03f * (xMax - xMin);
    float bar_z_start = zMin;  // Start at the same Z as the Z-axis
    float bar_z_end = zMax;    // End at the same Z as the Z-axis
    
    // Draw the color bar as a series of colored rectangles along Z-axis (vertical)
    int color_segments = 50;
    float segment_height = (bar_z_end - bar_z_start) / color_segments;
    
    for (int i = 0; i < color_segments; i++) {
        float z_value = bar_z_start + i * segment_height;  // Actual Z value for this segment
        float r, g, b;
        get_color_from_map(z_value, zMin, zMax, color_map, &r, &g, &b);
        glColor3f(r, g, b);
        
        float z_bottom = z_value;
        float z_top = z_bottom + segment_height;
        
        // Draw a colored quad for this segment (vertical along Z-axis)
        glBegin(GL_QUADS);
        glVertex3f(bar_x, bar_y, z_bottom);
        glVertex3f(bar_x + bar_width, bar_y, z_bottom);
        glVertex3f(bar_x + bar_width, bar_y, z_top);
        glVertex3f(bar_x, bar_y, z_top);
        glEnd();
    }
    
    // Draw border around colorbar
    glColor3f(1.0f, 1.0f, 1.0f);  // White border
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(bar_x, bar_y, bar_z_start);
    glVertex3f(bar_x + bar_width, bar_y, bar_z_start);
    glVertex3f(bar_x + bar_width, bar_y, bar_z_end);
    glVertex3f(bar_x, bar_y, bar_z_end);
    glEnd();
    glLineWidth(1.0f);
    
    // Determine overall exponent for scientific notation display (same logic as Z-axis)
    float abs_max_z = fmax(fabs(zMin), fabs(zMax));
    int overall_exponent = 0;
    bool use_scientific = false;
    if (abs_max_z != 0) {
        overall_exponent = (int)floor(log10(abs_max_z));
        overall_exponent = (overall_exponent / 3) * 3; // Engineering notation (same as Z-axis)
        use_scientific = (abs_max_z < 1e-3 || abs_max_z > 1e4);
    }
    
    // Add tick marks and labels (without individual scientific notation)
    int num_ticks = 5;
    for (int i = 0; i <= num_ticks; i++) {
        float frac = (float)i / num_ticks;
        float z_pos = bar_z_start + frac * (bar_z_end - bar_z_start);
        float z_value = zMin + frac * (zMax - zMin);
        
        // Draw tick mark
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex3f(bar_x + bar_width, bar_y, z_pos);
        glVertex3f(bar_x + bar_width + 0.02f * (xMax - xMin), bar_y, z_pos);
        glEnd();
        
        // Draw label (scaled by overall exponent if using scientific notation)
        char label[32];
        if (use_scientific) {
            float scaled_value = z_value / pow(10, overall_exponent);
            snprintf(label, sizeof(label), "%.2f", scaled_value);
        } else {
            snprintf(label, sizeof(label), "%.3f", z_value);
        }
        
        glRasterPos3f(bar_x + bar_width + 0.04f * (xMax - xMin), bar_y, z_pos);
        for (char* c = label; *c; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
        }
    }
    
    // Add colorbar title and scientific notation at the top
    float title_z_pos = bar_z_end + 0.05f * (zMax - zMin);
    glRasterPos3f(bar_x, bar_y, title_z_pos);
    for (const char* c = zlabel; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Add scientific notation beside the title if needed
    if (use_scientific) {
        char sci_notation[32];
        snprintf(sci_notation, sizeof(sci_notation), " x10^%d", overall_exponent);
        
        // Calculate approximate width of the title to position scientific notation beside it
        float title_width = strlen(zlabel) * 8.0f;  // Approximate character width
        float sci_x_pos = bar_x + title_width * 0.001f * (xMax - xMin);  // Scale to world coordinates
        
        glRasterPos3f(sci_x_pos, bar_y, title_z_pos);
        for (const char* c = sci_notation; *c; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
        }
    }
}

void draw_scene(void) {
    int width, height;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / height, 0.1, 100.0);
    
    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Black background
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, zoomZ);
    glRotatef(angleX, 1.0f, 0.0f, 0.0f);
    glRotatef(angleY, 0.0f, 1.0f, 0.0f);
    glRotatef(angleZ, 0.0f, 0.0f, 1.0f);

    float xMin = xgrid[0], xMax = xgrid[0];
    float yMin = ygrid[0], yMax = ygrid[0];
    float zMin = zdata[0][0], zMax = zdata[0][0];
    for (int i = 1; i < POINTS_PER_CURVE; ++i) {
        if (xgrid[i] < xMin) xMin = xgrid[i];
        if (xgrid[i] > xMax) xMax = xgrid[i];
    }
    for (int j = 1; j < NUM_CURVES; ++j) {
        if (ygrid[j] < yMin) yMin = ygrid[j];
        if (ygrid[j] > yMax) yMax = ygrid[j];
    }
    for (int j = 0; j < NUM_CURVES; ++j) {
        for (int i = 0; i < POINTS_PER_CURVE; ++i) {
            float z = zdata[j][i];
            if (z < zMin) zMin = z;
            if (z > zMax) zMax = z;
        }
    }

    float centerX = 0.5f * (xMin + xMax);
    float centerY = 0.5f * (yMin + yMax);
    float centerZ = 0.5f * (zMin + zMax);

    float scaleX = 2.0f / (xMax - xMin);
    float scaleY = 2.0f / (yMax - yMin);
    float scaleZ = 2.0f / (zMax - zMin);

    glScalef(scaleX, scaleY, scaleZ);
    glTranslatef(-centerX, -centerY, -centerZ);

    draw_axes(xMin, xMax, yMin, yMax, zMin, zMax);

    if (surface_mode) {
        // Draw 3D surface with contours (similar to gnuplot)
        
        // First, draw the surface using triangular mesh
        for (int j = 0; j < NUM_CURVES - 1; ++j) {
            glBegin(GL_TRIANGLE_STRIP);
            for (int i = 0; i < POINTS_PER_CURVE; ++i) {
                // Bottom vertex
                float z1 = zdata[j][i];
                float r1, g1, b1;
                get_color_from_map(z1, zMin, zMax, color_map, &r1, &g1, &b1);
                glColor3f(r1, g1, b1);
                glVertex3f(xgrid[i], ygrid[j], z1);
                
                // Top vertex
                float z2 = zdata[j+1][i];
                float r2, g2, b2;
                get_color_from_map(z2, zMin, zMax, color_map, &r2, &g2, &b2);
                glColor3f(r2, g2, b2);
                glVertex3f(xgrid[i], ygrid[j+1], z2);
            }
            glEnd();
        }
        
        // Draw contour lines
        glLineWidth(1.0f);
        glColor3f(0.2f, 0.2f, 0.2f);  // Dark gray contour lines
        
        // Horizontal contour lines (constant Y)
        for (int j = 0; j < NUM_CURVES; j += 3) {  // Every 3rd curve for clarity
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i < POINTS_PER_CURVE; ++i) {
                glVertex3f(xgrid[i], ygrid[j], zdata[j][i]);
            }
            glEnd();
        }
        
        // Vertical contour lines (constant X)
        for (int i = 0; i < POINTS_PER_CURVE; i += 5) {  // Every 5th point for clarity
            glBegin(GL_LINE_STRIP);
            for (int j = 0; j < NUM_CURVES; ++j) {
                glVertex3f(xgrid[i], ygrid[j], zdata[j][i]);
            }
            glEnd();
        }
        
        // Draw colorbar for surface mode (both single-page and multi-page)
        draw_colorbar(xMin, xMax, yMin, yMax, zMin, zMax);
        
    } else {
        // Draw waterfall curves using the selected color map
        glLineWidth(2.2f);  // Slightly thicker for better visibility
        for (int j = 0; j < NUM_CURVES; ++j) {
            // Use Y position (ygrid[j]) to determine color based on the selected color map
            float r, g, b;
            get_color_from_map(ygrid[j], ygrid[0], ygrid[NUM_CURVES-1], color_map, &r, &g, &b);
            glColor3f(r, g, b);

            glBegin(GL_LINE_STRIP);
            for (int i = 0; i < POINTS_PER_CURVE; ++i) {
                glVertex3f(xgrid[i], ygrid[j], zdata[j][i]);
            }
            glEnd();
        }
        
        // Draw colorbar for waterfall mode (same as surface mode)
        draw_colorbar(xMin, xMax, yMin, yMax, zMin, zMax);
    }
    
    glLineWidth(1.0f);
}

// Helper for scientific notation tick formatting
void format_tick(float val, char* buf, size_t buflen) {
    if (fabs(val) < 1e-3 || fabs(val) > 1e4) {
        int exp = (int)floor(log10(fabs(val)));
        float base = val / pow(10, exp);
        snprintf(buf, buflen, "%.2fe%d", base, exp);
    } else {
        snprintf(buf, buflen, "%.2f", val);
    }
}

void save_frame(const char* filename, int width, int height) {
    uint8_t* pixels = malloc(3 * width * height);
    if (!pixels) return;
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // Flip the image vertically (OpenGL origin is bottom-left, PNG expects top-left)
    uint8_t* flipped_pixels = malloc(3 * width * height);
    if (!flipped_pixels) {
        free(pixels);
        return;
    }
    
    for (int y = 0; y < height; y++) {
        memcpy(flipped_pixels + 3 * y * width, 
               pixels + 3 * (height - 1 - y) * width, 
               3 * width);
    }

    // Save as PNG
    stbi_write_png(filename, width, height, 3, flipped_pixels, width * 3);
    
    free(pixels);
    free(flipped_pixels);
}

void create_movie_from_frames() {
    if (!movie.enabled || strcmp(movie.format, "frames") == 0) {
        return;  // No movie conversion needed
    }
    
    char command[1024];
    char input_pattern[512];
    char output_file[512];
    
    // Construct input pattern and output filename
    snprintf(input_pattern, sizeof(input_pattern), "%s/frame_%%06d.png", movie.temp_dir);
    
    if (strcmp(movie.format, "mp4") == 0) {
        snprintf(output_file, sizeof(output_file), "%s.mp4", movie.filename);
        snprintf(command, sizeof(command), 
                "ffmpeg -y -framerate %d -i %s -c:v libx264 -pix_fmt yuv420p -crf 18 %s 2>/dev/null",
                movie.fps, input_pattern, output_file);
    } else if (strcmp(movie.format, "gif") == 0) {
        snprintf(output_file, sizeof(output_file), "%s.gif", movie.filename);
        // Create high-quality GIF using ffmpeg
        snprintf(command, sizeof(command), 
                "ffmpeg -y -framerate %d -i %s -vf \"fps=%d,scale=%d:%d:flags=lanczos,palettegen\" %s/palette.png 2>/dev/null && "
                "ffmpeg -y -framerate %d -i %s -i %s/palette.png -lavfi \"fps=%d,scale=%d:%d:flags=lanczos[x];[x][1:v]paletteuse\" %s 2>/dev/null",
                movie.fps, input_pattern, movie.fps, movie.width, movie.height, movie.temp_dir,
                movie.fps, input_pattern, movie.temp_dir, movie.fps, movie.width, movie.height, output_file);
    }
    
    printf("Creating %s movie: %s\n", movie.format, output_file);
    int result = system(command);
    
    if (result == 0) {
        printf("Movie created successfully: %s\n", output_file);
    } else {
        printf("Warning: Movie creation failed. Check if ffmpeg is installed.\n");
        printf("Individual frames are available in: %s\n", movie.temp_dir);
    }
}

void cleanup_temp_files() {
    if (!movie.enabled || strcmp(movie.format, "frames") == 0) {
        return;  // Keep frames if format is "frames"
    }
    
    char command[512];
    snprintf(command, sizeof(command), "rm -rf %s", movie.temp_dir);
    system(command);
    printf("Temporary files cleaned up.\n");
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);  // REQUIRED for glutBitmapCharacter
    SCANNED_ARG *s_arg;
    char *xcol, *ycol, *zcol, *inputfile, *outputfile;
    long i_arg;
    unsigned long dummyFlags;
    int n_rows, i;
    
    xcol = ycol = zcol = inputfile = outputfile =  NULL;
    
    // Initialize movie parameters with defaults
    movie.enabled = false;
    strcpy(movie.format, "mp4");
    strcpy(movie.filename, "rotation_movie");
    movie.fps = 10;
    movie.width = 1200;
    movie.height = 900;
    
    argc = scanargs(&s_arg, argc, argv);
    
    if (argc < 2) {
        print_usage();
        return 1;
    }
    for (i_arg=1; i_arg < argc; i_arg++) {
      if (s_arg[i_arg].arg_type == OPTION) {
	delete_chars(s_arg[i_arg].list[0], "_");
	/* process options here */
	switch (match_string(s_arg[i_arg].list[0], option, OPTIONS, 0)) {
	case SET_SINGLEPAGE:
	  single_page=true;
	  if (s_arg[i_arg].n_items <4) {
	     fprintf(stderr, "Error (sddswaterfall): invalid -xyz syntax\n");
	     return (1);
	  }
	  xcol = s_arg[i_arg].list[1];
	  ycol = s_arg[i_arg].list[2];
	  zcol = s_arg[i_arg].list[3];
	  break;
	case SET_MULTIPAGE:
	  multi_page=true;
	  if (s_arg[i_arg].n_items <4) {
	     fprintf(stderr, "Error (sddswaterfall): invalid -xyz syntax\n");
	     return (1);
	  }
	  /*multipage data, two columns: xcol, zcol, and y data is from the parameter, the third*/
	  xcol = s_arg[i_arg].list[1];
	  zcol = s_arg[i_arg].list[2];
	  ycol = s_arg[i_arg].list[3]; /*it is actually the parameter name */
	  break;
	case SET_OUTPUT:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -outputFile syntax\n");
	    return (1);
	  }
	  outputfile = s_arg[i_arg].list[1];
	  break;
	case SET_SURFACE:
	  surface_mode = true;
	  break;
	case SET_GNUPLOTSURFACE:
	  gnuplot_surface = true;
	  break;
	case SET_VIEWANGLE:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -viewAngle syntax\n");
	    return (1);
	  }
	  s_arg[i_arg].n_items--;
	  if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
			    "x", SDDS_DOUBLE, &angleX, 1, 0,
			    "y", SDDS_DOUBLE, &angleY, 1, 0,
			    "z", SDDS_DOUBLE, &angleZ, 1, 0,
			    NULL)) {
	    fprintf(stderr, "Error (sddscontour): invalid -viewAngle syntax/values\n");
	    return (1);
	  }
	  s_arg[i_arg].n_items++;
	  break;
	case SET_ZOOM:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -zoom syntax\n");
	    return (1);
	  }
	  if (!get_double(&zoomZ, s_arg[i_arg].list[1])) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -zoom syntax/values\n");
	    return (1);
	  }
	  break;
	case SET_ROTATEVIEW:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -rotateView syntax\n");
	    return (1);
	  }
	  s_arg[i_arg].n_items--;
	  char *axis_str = NULL;
	  if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
			    "axis", SDDS_STRING, &axis_str, 1, 0,
			    "min", SDDS_DOUBLE, &rotation.min_angle, 1, 0,
			    "max", SDDS_DOUBLE, &rotation.max_angle, 1, 0,
			    "positions", SDDS_LONG, &rotation.positions, 1, 0,
			    "pause", SDDS_DOUBLE, &rotation.pause, 1, 0,
			    NULL)) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -rotateView syntax/values\n");
	    return (1);
	  }
	  s_arg[i_arg].n_items++;
	  
	  // Extract axis character
	  if (!axis_str || strlen(axis_str) != 1) {
	    fprintf(stderr, "Error (sddswaterfall): rotateView axis must be 'x', 'y', or 'z'\n");
	    return (1);
	  }
	  rotation.axis = axis_str[0];
	  
	  // Validate axis parameter
	  if (rotation.axis != 'x' && rotation.axis != 'y' && rotation.axis != 'z') {
	    fprintf(stderr, "Error (sddswaterfall): rotateView axis must be 'x', 'y', or 'z'\n");
	    return (1);
	  }
	  
	  // Validate other parameters
	  if (rotation.positions < 2) {
	    fprintf(stderr, "Error (sddswaterfall): rotateView positions must be >= 2\n");
	    return (1);
	  }
	  if (rotation.pause < 0.01) {
	    fprintf(stderr, "Error (sddswaterfall): rotateView pause must be >= 0.1 seconds\n");
	    return (1);
	  }
	  
	  rotation.enabled = true;
	  rotation.current_pos = 0;
	  rotation.direction = 1;
	  rotation.last_time = 0.0;
	  
	  printf("Rotation enabled: axis=%c, range=[%.1f,%.1f], positions=%d, pause=%.1fs\n",
	         rotation.axis, rotation.min_angle, rotation.max_angle, 
	         rotation.positions, rotation.pause);
	  break;
	case SET_MOVIEEXPORT:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -movieExport syntax\n");
	    return (1);
	  }
	  s_arg[i_arg].n_items--;
	  char *format_str = NULL, *filename_str = NULL;
	  long fps_val = 0, width_val = 0, height_val = 0;
	  if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
			    "format", SDDS_STRING, &format_str, 1, 0,
			    "filename", SDDS_STRING, &filename_str, 1, 0,
			    "fps", SDDS_LONG, &fps_val, 1, 0,
			    "width", SDDS_LONG, &width_val, 1, 0,
			    "height", SDDS_LONG, &height_val, 1, 0,
			    NULL)) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -movieExport syntax/values\n");
	    return (1);
	  }
	  s_arg[i_arg].n_items++;
	  
	  movie.enabled = true;
	  
	  if (format_str) {
	    if (strcmp(format_str, "mp4") != 0 && strcmp(format_str, "gif") != 0 && strcmp(format_str, "frames") != 0) {
	      fprintf(stderr, "Error (sddswaterfall): movieExport format must be 'mp4', 'gif', or 'frames'\n");
	      return (1);
	    }
	    strcpy(movie.format, format_str);
	  }
	  
	  if (filename_str) {
	    strcpy(movie.filename, filename_str);
	  }
	  
	  if (fps_val > 0) {
	    movie.fps = fps_val;
	  }
	  
	  if (width_val > 0) {
	    movie.width = width_val;
	  }
	  
	  if (height_val > 0) {
	    movie.height = height_val;
	  }
	  
	  printf("Movie export enabled: format=%s, filename=%s, fps=%d, size=%dx%d\n",
	         movie.format, movie.filename, movie.fps, movie.width, movie.height);
	  break;
	case SET_SWAPXY:
	  swap_xy = true;
	  break;
	case SET_CMAP:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error: invalid -cmap syntax\n");
	    fprintf(stderr, "Usage: -cmap=<colormap>\n");
	    fprintf(stderr, "Available colormaps: 'jet', 'coolwarm', 'viridis', 'plasma'\n");
	    fprintf(stderr, "Example: -cmap=plasma\n");
	    exit(1);
	  }
	  if (strcmp(s_arg[i_arg].list[1], "jet") == 0) {
	    color_map = CMAP_JET;
	  } else if (strcmp(s_arg[i_arg].list[1], "coolwarm") == 0) {
	    color_map = CMAP_COOLWARM;
	  } else if (strcmp(s_arg[i_arg].list[1], "viridis") == 0) {
	    color_map = CMAP_VIRIDIS;
	  } else if (strcmp(s_arg[i_arg].list[1], "plasma") == 0) {
	    color_map = CMAP_PLASMA;
	  } else {
	    fprintf(stderr, "Error: invalid colormap '%s'\n", s_arg[i_arg].list[1]);
	    fprintf(stderr, "Usage: -cmap=<colormap>\n");
	    fprintf(stderr, "Available colormaps: 'jet', 'coolwarm', 'viridis', 'plasma'\n");
	    fprintf(stderr, "Example: -cmap=plasma\n");
	    exit(1);
	  }
	  break;
	case SET_XSCALE:
	  if (s_arg[i_arg].n_items < 3) {
	    fprintf(stderr, "Error: invalid -xscale syntax\n");
	    exit(1);
	  }
	  if (!get_double(&xscale_min, s_arg[i_arg].list[1]) ||
	      !get_double(&xscale_max, s_arg[i_arg].list[2])) {
	    fprintf(stderr, "Error: invalid -xscale values\n");
	    exit(1);
	  }
	  xscale_set = true;
	  break;
	case SET_YSCALE:
	  if (s_arg[i_arg].n_items < 3) {
	    fprintf(stderr, "Error: invalid -yscale syntax\n");
	    exit(1);
	  }
	  if (!get_double(&yscale_min, s_arg[i_arg].list[1]) ||
	      !get_double(&yscale_max, s_arg[i_arg].list[2])) {
	    fprintf(stderr, "Error: invalid -yscale values\n");
	    exit(1);
	  }
	  yscale_set = true;
	  break;
	case SET_XLABEL:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -xlabel syntax\n");
	    return (1);
	  }
	  strncpy(xlabel, s_arg[i_arg].list[1], sizeof(xlabel)-1);
	  xlabel[sizeof(xlabel)-1] = '\0';
	  break;
	case SET_YLABEL:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -ylabel syntax\n");
	    return (1);
	  }
	  strncpy(ylabel, s_arg[i_arg].list[1], sizeof(ylabel)-1);
	  ylabel[sizeof(ylabel)-1] = '\0';
	  break;
	case SET_ZLABEL:
	  if (s_arg[i_arg].n_items < 2) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -zlabel syntax\n");
	    return (1);
	  }
	  strncpy(zlabel, s_arg[i_arg].list[1], sizeof(zlabel)-1);
	  zlabel[sizeof(zlabel)-1] = '\0';
	  break;
	case SET_LABELSCALE:
	  if (s_arg[i_arg].n_items < 4) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -labelScale syntax\n");
	    fprintf(stderr, "Usage: -labelScale=<xlabelScale>,<ylabelScale>,<zlabelScale>\n");
	    return (1);
	  }
	  if (!get_double(&xlabel_scale, s_arg[i_arg].list[1]) ||
	      !get_double(&ylabel_scale, s_arg[i_arg].list[2]) ||
	      !get_double(&zlabel_scale, s_arg[i_arg].list[3])) {
	    fprintf(stderr, "Error (sddswaterfall): invalid -labelScale values\n");
	    return (1);
	  }
	  if (xlabel_scale <= 0.0 || ylabel_scale <= 0.0 || zlabel_scale <= 0.0) {
	    fprintf(stderr, "Error (sddswaterfall): labelScale values must be positive\n");
	    return (1);
	  }
	  printf("Label scaling set: X=%.2f, Y=%.2f, Z=%.2f\n", 
	         xlabel_scale, ylabel_scale, zlabel_scale);
	  break;
	default:
	  fprintf(stderr, "unknown option - %s given.\n", s_arg[i_arg].list[0]);
	  exit(1);
	  break;
	}
      } else {
	if (inputfile)
	  bomb("only one filename accepted", NULL);
	inputfile = s_arg[i_arg].list[0];
      }
    }
    if (!inputfile) {
        fprintf(stderr, "Error: No input file specified.\n");
        return 1;
    }
    
    if (!xcol || !ycol || !zcol) {
        fprintf(stderr, "Error: -singlePage or multiPage is required. Please specify column/parameter names for X, Y, and Z data.\n");
        return 1;
    }
    if (single_page && multi_page) {
      fprintf(stderr, "Error: singlePage and multiPage option can not both be provided!");
      return 1;
    }
    
    // Validate movie export requirements
    if (movie.enabled && !rotation.enabled) {
        fprintf(stderr, "Error: -movieExport requires -rotateView to be enabled.\n");
        return 1;
    }
    // Set default output filename if not specified
    if (!outputfile) {
        outputfile = "screenshot.png";
    }
    
    // Set axis labels from column names only if not already set by command line options
    if (strcmp(xlabel, "X") == 0) {  // Only set if still default value
        strncpy(xlabel, xcol, sizeof(xlabel)-1);
        xlabel[sizeof(xlabel)-1] = '\0';
    }
    if (strcmp(ylabel, "Y") == 0) {  // Only set if still default value
        strncpy(ylabel, ycol, sizeof(ylabel)-1);
        ylabel[sizeof(ylabel)-1] = '\0';
    }
    if (strcmp(zlabel, "Z") == 0) {  // Only set if still default value
        strncpy(zlabel, zcol, sizeof(zlabel)-1);
        zlabel[sizeof(zlabel)-1] = '\0';
    }
    
    // Setup movie export if enabled
    if (movie.enabled) {
        // Create temporary directory for frames
        snprintf(movie.temp_dir, sizeof(movie.temp_dir), "/tmp/sddswaterfall_frames_%d", getpid());
        mkdir(movie.temp_dir, 0755);
        
        rotation.movie_mode = true;
        rotation.save_frames = true;
        rotation.frame_count = 0;
        
        // Calculate total frames for complete rotation cycle (back and forth)
        rotation.total_frames = 2 * (rotation.positions - 1);
        
        printf("Movie recording will capture %d frames in directory: %s\n", 
               rotation.total_frames, movie.temp_dir);
    }
    
    // Validate that only one surface mode is selected
    if (surface_mode && gnuplot_surface) {
        fprintf(stderr, "Error: Cannot specify both -surface and -gnuplotSurface at the same time.\n");
        return 1;
    }
    load_sdds_data(inputfile, xcol, ycol, zcol);

    if (!multi_page) {
      n_rows = pages[0].n_points;
      Sample samples[n_rows];
      for (int i = 0; i < n_rows; ++i) {
        samples[i].x = (float)pages[0].x_data[i];
        samples[i].y = (float)pages[0].y_data[i];
        samples[i].z = (float)pages[0].z_data[i];
      }
      
      float x_min = samples[0].x, x_max = samples[0].x;
      float y_min = samples[0].y, y_max = samples[0].y;
      for (int i = 1; i < n_rows; ++i) {
        if (samples[i].x < x_min) x_min = samples[i].x;
        if (samples[i].x > x_max) x_max = samples[i].x;
        if (samples[i].y < y_min) y_min = samples[i].y;
        if (samples[i].y > y_max) y_max = samples[i].y;
      }
     
      for (int i = 0; i < POINTS_PER_CURVE; ++i)
        xgrid[i] = x_min + i * (x_max - x_min) / (POINTS_PER_CURVE - 1);
      for (int j = 0; j < NUM_CURVES; ++j)
        ygrid[j] = y_min + j * (y_max - y_min) / (NUM_CURVES - 1);
      
      interpolate_grid(samples, n_rows, xgrid, POINTS_PER_CURVE, ygrid, NUM_CURVES, zdata);
    } else {
      /*for multi-page data */
      prepare_multi_page_grid_data();
       // Handle axis swapping for multi-page data
    }
    if (swap_xy) {
      // Swap X and Y grids
      float temp_grid[NUM_CURVES];
      for (int i = 0; i < NUM_CURVES; i++) {
	temp_grid[i] = ygrid[i];
      }
      for (int i = 0; i < POINTS_PER_CURVE && i < NUM_CURVES; i++) {
	ygrid[i] = xgrid[i];
      }
      for (int i = 0; i < NUM_CURVES && i < POINTS_PER_CURVE; i++) {
	xgrid[i] = temp_grid[i];
      }
      // Transpose zdata matrix
      float temp_zdata[NUM_CURVES][POINTS_PER_CURVE];
      for (int j = 0; j < NUM_CURVES; j++) {
	for (int i = 0; i < POINTS_PER_CURVE; i++) {
	  temp_zdata[j][i] = zdata[j][i];
	}
      }
      for (int j = 0; j < NUM_CURVES && j < POINTS_PER_CURVE; j++) {
	for (int i = 0; i < POINTS_PER_CURVE && i < NUM_CURVES; i++) {
	  zdata[j][i] = temp_zdata[i][j];
	}
      }
	 // Swap axis labels
      char temp_label[128];
      strncpy(temp_label, xlabel, sizeof(temp_label)-1);
      strncpy(xlabel, ylabel, sizeof(xlabel)-1);
      strncpy(ylabel, temp_label, sizeof(ylabel)-1);
      temp_label[sizeof(temp_label)-1] = '\0';
      xlabel[sizeof(xlabel)-1] = '\0';
      ylabel[sizeof(ylabel)-1] = '\0';
    }
    
    // If gnuplot surface mode, generate the plot and exit
    if (gnuplot_surface) {
      if (rotation.enabled)
	fprintf(stdout, "Rotation view is ignored for gnuplot.\n");
      generate_gnuplot_surface();
        return 0;
    }
      
    // Fork the process to run OpenGL in background (only if not recording movie)
    if (!movie.enabled) {
      pid_t pid = fork();
      if (pid < 0) {
	fprintf(stderr, "Failed to fork process.\n");
	return -1;
      } else if (pid > 0) {
	// Parent process - exit immediately to return control to terminal
	printf("3D waterfall plot launched in background (PID: %d)\n", pid);
	return 0;
      }
        
      // Child process continues with OpenGL rendering
      // Detach from terminal
      setsid();
        
      // Redirect stdout and stderr to /dev/null to suppress X connection messages
      freopen("/dev/null", "w", stdout);
      freopen("/dev/null", "w", stderr);
    }
    
    if (!glfwInit()) {
      fprintf(stderr, "Failed to initialize GLFW.\n");
      return -1;
    }
    
    // Use specified movie dimensions if recording
    int window_width = movie.enabled ? movie.width : 1200;
    int window_height = movie.enabled ? movie.height : 900;
      
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Figure 1", NULL, NULL);
    if (!window) {
      fprintf(stderr, "Failed to create window.\n");
      glfwTerminate();
      return -1;
    }
    glfwMakeContextCurrent(window);
    glEnable(GL_DEPTH_TEST);
      
    float lastX = 400, lastY = 300;
    int leftButtonPressed = 0;
    glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, 1);
      
    while (!glfwWindowShouldClose(window)) {
      // Handle rotation animation
      if (rotation.enabled) {
	double current_time = glfwGetTime();
	if (rotation.last_time == 0.0) {
	  rotation.last_time = current_time;
	}
	
	if (current_time - rotation.last_time >= rotation.pause) {
	  // Calculate the current angle based on position
	  double angle_range = rotation.max_angle - rotation.min_angle;
	  double t = (double)rotation.current_pos / (rotation.positions - 1);
	  double current_angle = rotation.min_angle + t * angle_range;
            
	  // Apply rotation to the appropriate axis
	  switch (rotation.axis) {
	  case 'x':
	    angleX = current_angle;
	    break;
	  case 'y':
	    angleY = current_angle;
	    break;
	  case 'z':
	    angleZ = current_angle;
	    break;
	  }
            
	  // Save frame if movie recording is enabled
	  if (rotation.save_frames && movie.enabled) {
	    char frame_filename[512];
	    snprintf(frame_filename, sizeof(frame_filename), 
		     "%s/frame_%06d.png", movie.temp_dir, rotation.frame_count);
	    
	    // Render the scene first
	    draw_scene();
	    glfwSwapBuffers(window);
                  
	    // Save the frame
	    int width, height;
	    glfwGetFramebufferSize(window, &width, &height);
	    save_frame(frame_filename, width, height);
	    
	    rotation.frame_count++;
                  
	    if (!movie.enabled) {  // Only print progress if not in background mode
	      printf("Saved frame %d/%d\n", rotation.frame_count, rotation.total_frames);
	    }
	  }
                
	  // Update position for next frame
	  rotation.current_pos += rotation.direction;
                
	  // Reverse direction at endpoints for back-and-forth motion
	  if (rotation.current_pos >= rotation.positions - 1) {
	    rotation.current_pos = rotation.positions - 1;
	    rotation.direction = -1;
	  } else if (rotation.current_pos <= 0) {
	    rotation.current_pos = 0;
	    rotation.direction = 1;
	  }
                
	  rotation.last_time = current_time;
                
	  // Check if movie recording is complete
	  if (movie.enabled && rotation.frame_count >= rotation.total_frames) {
	    printf("Movie recording complete. Creating %s file...\n", movie.format);
	    create_movie_from_frames();
	    cleanup_temp_files();
	    glfwSetWindowShouldClose(window, GLFW_TRUE);
	  }
	}
      }
        
      draw_scene();
      glfwSwapBuffers(window);
        
      // Manual mouse controls (disabled during rotation animation)
      if (!rotation.enabled) {
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
	  if (!leftButtonPressed) {
	    leftButtonPressed = 1;
	    double xpos, ypos;
	    glfwGetCursorPos(window, &xpos, &ypos);
	    lastX = xpos;
	    lastY = ypos;
	  } else {
	    double xpos, ypos;
	    glfwGetCursorPos(window, &xpos, &ypos);
	    angleY += (xpos - lastX) * 0.5f;
	    angleX += (ypos - lastY) * 0.5f;
	    lastX = xpos;
	    lastY = ypos;
	  }
	} else {
	  leftButtonPressed = 0;
	}
      }
        
      // Zoom controls always available
      if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) zoomZ += 0.05f;
      if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) zoomZ -= 0.05f;
        
      // Allow ESC key to stop rotation
      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && rotation.enabled) {
	rotation.enabled = false;
	if (!movie.enabled) {  // Only print if not in background mode
	  printf("Rotation animation stopped. Manual controls enabled.\n");
	}
      }
        
      glfwPollEvents();
    }

    // Save final screenshot if not in movie mode
    if (!movie.enabled) {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      save_frame(outputfile, width, height);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    for (i=0; i<n_pages; i++) {
      if (pages[i].x_data) free(pages[i].x_data);
      if (pages[i].y_data) free(pages[i].y_data);
      if (pages[i].z_data) free(pages[i].z_data);
    }
    free(pages);
    return 0;
}

// Prepare multi-page grid data for visualization
void prepare_multi_page_grid_data() {
    if (n_pages == 0) {
        fprintf(stderr, "Error: No pages loaded for multi-page processing\n");
        exit(1);
    }
    
    printf("Preparing multi-page grid data from %d pages\n", n_pages);
    
    // Find global X range across all pages
    double x_min = pages[0].x_data[0], x_max = pages[0].x_data[0];
    for (int page = 0; page < n_pages; page++) {
        for (int i = 0; i < pages[page].n_points; i++) {
            if (pages[page].x_data[i] < x_min) x_min = pages[page].x_data[i];
            if (pages[page].x_data[i] > x_max) x_max = pages[page].x_data[i];
        }
    }
    
    // Apply X-scale limits if specified
    if (xscale_set) {
        x_min = xscale_min;
        x_max = xscale_max;
    }
    
    // Create uniform X grid
    for (int i = 0; i < POINTS_PER_CURVE; i++) {
        xgrid[i] = x_min + i * (x_max - x_min) / (POINTS_PER_CURVE - 1);
    }
    
    // Collect parameter values and sort them
    double *param_values = malloc(n_pages * sizeof(double));
    for (int page = 0; page < n_pages; page++) {
        param_values[page] = pages[page].z_param;
    }
    
    // Simple bubble sort for parameter values
    for (int i = 0; i < n_pages - 1; i++) {
        for (int j = 0; j < n_pages - i - 1; j++) {
            if (param_values[j] > param_values[j + 1]) {
                double temp = param_values[j];
                param_values[j] = param_values[j + 1];
                param_values[j + 1] = temp;
            }
        }
    }
    
    // Find parameter range
    double param_min = param_values[0];
    double param_max = param_values[n_pages - 1];
    fprintf(stderr, "current min %f max %f \n", param_min, param_max);
    // Apply Y-scale limits if specified (parameter axis)
    if (yscale_set) {
        param_min = yscale_min;
        param_max = yscale_max;
    }
    fprintf(stderr, "yscale min %f max %f \n", param_min, param_max);
    // Create Y grid based on parameter values
    /* Y grid spanning full parameter range */
    if (n_pages <= NUM_CURVES) {
      /* Use all actual parameter values (sorted), then fill any tail by interpolation */
      int j = 0;
      for (; j < n_pages && j < NUM_CURVES; ++j)
        ygrid[j] = param_values[j];
      for (; j < NUM_CURVES; ++j)
        ygrid[j] = param_min + (param_max - param_min) * j / (NUM_CURVES - 1);
    } else {
      /* More pages than curves: sample evenly across the full param range */
      for (int j = 0; j < NUM_CURVES; ++j) {
        int idx = (int)llround(j * (n_pages - 1.0) / (NUM_CURVES - 1.0));
        if (idx < 0) idx = 0;
        if (idx > n_pages - 1) idx = n_pages - 1;
        ygrid[j] = param_values[idx];
      }
    }
    
    // Initialize zdata array
    for (int j = 0; j < NUM_CURVES; j++) {
        for (int i = 0; i < POINTS_PER_CURVE; i++) {
            zdata[j][i] = 0.0f;
        }
    }
    
    // Process each page and interpolate onto the grid
    for (int page = 0; page < n_pages; page++) {
        double page_param = pages[page].z_param;
        
        // Skip pages outside parameter range
        if (yscale_set && (page_param < param_min || page_param > param_max)) {
            continue;
        }
        
        // Find the closest Y grid position for this page's parameter value
        int best_j = 0;
        double min_dist = fabs(ygrid[0] - page_param);
        for (int j = 1; j < NUM_CURVES; j++) {
            double dist = fabs(ygrid[j] - page_param);
            if (dist < min_dist) {
                min_dist = dist;
                best_j = j;
            }
        }
        
        // Interpolate this page's data onto the X grid
        for (int i = 0; i < POINTS_PER_CURVE; i++) {
            double x_target = xgrid[i];
            
            // Skip X values outside the specified range
            if (xscale_set && (x_target < x_min || x_target > x_max)) {
                continue;
            }
            
            // Find interpolated Y value (spectrum) for this X position
            double interpolated_y = 0.0;
            
            // Simple linear interpolation
            if (pages[page].n_points >= 2) {
                // Find the two closest X points
                int idx1 = 0, idx2 = pages[page].n_points - 1;
                
                for (int k = 0; k < pages[page].n_points - 1; k++) {
                    if (pages[page].x_data[k] <= x_target && pages[page].x_data[k + 1] >= x_target) {
                        idx1 = k;
                        idx2 = k + 1;
                        break;
                    }
                }
                
                // Linear interpolation
                if (idx2 > idx1 && pages[page].x_data[idx2] != pages[page].x_data[idx1]) {
                    double t = (x_target - pages[page].x_data[idx1]) / 
                              (pages[page].x_data[idx2] - pages[page].x_data[idx1]);
                    interpolated_y = pages[page].y_data[idx1] + 
                                   t * (pages[page].y_data[idx2] - pages[page].y_data[idx1]);
                } else {
                    // Use nearest neighbor if interpolation fails
                    interpolated_y = pages[page].y_data[idx1];
                }
            } else if (pages[page].n_points == 1) {
                interpolated_y = pages[page].y_data[0];
            }
            
            // Store the interpolated spectrum value
            zdata[best_j][i] = (float)interpolated_y;
        }
    }
    
    free(param_values);
    printf("Multi-page grid data preparation complete\n");
}

    // Load SDDS data
void load_sdds_data(char *filename, char *xcol, char *ycol, char *zcol) {
  SDDS_DATASET SDDS_table;
  int n_rows;
  
  if (!SDDS_InitializeInput(&SDDS_table, filename)) {
    fprintf(stderr, "Error: Failed to initialize SDDS input for file: %s\n", filename);
    exit(1);
  }
  
  // Count pages first
  n_pages = 0;
  while (SDDS_ReadPage(&SDDS_table) > 0) {
    n_rows = SDDS_CountRowsOfInterest(&SDDS_table);
    if (n_rows == 0) continue;
    n_pages++;
    pages = SDDS_Realloc(pages, n_pages * sizeof(PageData));
    pages[n_pages-1].x_data = pages[n_pages-1].y_data = pages[n_pages-1].z_data = NULL;
    if (!pages) {
      fprintf(stderr, "Error: Failed to allocate memory for page data\n");
      exit(1);
    }
    pages[n_pages-1].n_points = n_rows;
    if (multi_page) {
      if (!(pages[n_pages-1].x_data = SDDS_GetColumnInDoubles(&SDDS_table, xcol))) {
	fprintf(stderr, "Error: Failed to get column data %s\n", xcol );
	exit(1);
      }
      if (!(pages[n_pages-1].y_data = SDDS_GetColumnInDoubles(&SDDS_table, zcol))) {
	fprintf(stderr, "Error: Failed to get column data %s\n", ycol );
	exit(1);
      }
      /*for multipage option, -multipage=<xcol>,<zcol>,<parameter> but ycol is assigned to <parameter>*/
      if (!SDDS_GetParameterAsDouble(&SDDS_table, ycol, &pages[n_pages-1].z_param)) {
	fprintf(stderr, "Error: Failed to get parameter data %s\n", zcol );
	exit(1);
      }
    } else {
      if (!(pages[n_pages-1].x_data = SDDS_GetColumnInDoubles(&SDDS_table, xcol))) {
	fprintf(stderr, "Error: Failed to get column data %s\n", xcol );
	exit(1);
      }
      if (!(pages[n_pages-1].y_data = SDDS_GetColumnInDoubles(&SDDS_table, ycol))) {
	fprintf(stderr, "Error: Failed to get column data %s\n", ycol );
	exit(1);
      }
      if (!(pages[n_pages-1].z_data = SDDS_GetColumnInDoubles(&SDDS_table, zcol))) {
	fprintf(stderr, "Error: Failed to get column data %s\n", ycol );
	exit(1);
      }
      break;
    }
  }
  if (!SDDS_Terminate(&SDDS_table)) {
    fprintf(stderr, "Error: Failed to terminate SDDS input\n");
    exit(1);
  }
  if (n_pages == 0) {
    fprintf(stderr, "Error: No valid pages loaded\n");
    exit(1);
  }
  printf("Successfully loaded %d pages\n", n_pages);
}
