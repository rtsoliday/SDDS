/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* Fallback resources for MPL Applications */

static String fallbackResources[] = {
  "MPLDriver*allowShellResize:                  True",
  "MPLDriver*background:                        SteelBlue3",
  "MPLDriver*foreground:                        Black",
  "MPLDriver*borderWidth:                       0",
  "MPLDriver*highlightThickness:		2",
  "MPLDriver*shadowThickness:			2",
  "MPLDriver*title:				MPL",
  "MPLDriver*traversalOn:			True",
  "MPLDriver*XmTextField.columns:		15",
  "MPLDriver*graphArea.background:		White",
  "MPLDriver*graphArea.borderWidth:		1",
  "MPLDriver*informationMessage*background:	SteelBlue2",
  "MPLDriver*mainMenu*background:		SteelBlue3",
  "MPLDriver*mainMenu*foreground:		White",
  "MPLDriver*mainMenu*marginHeight:		2",
  "MPLDriver*mainWindow*marginHeight:		2",
  "MPLDriver*mainWindow*marginWidth:		10",
  "MPLDriver*mainWindow.bottomOffset:		2",
  "MPLDriver*mainWindow.leftOffset:		2",
  "MPLDriver*mainWindow.rightOffset:		2",
  "MPLDriver*mainWindow.topOffset:		2",
  "MPLDriver*optPopup*background:		SteelBlue2",
  "MPLDriver*optPopup*marginHeight:		2",
  "MPLDriver*optPopup*marginWidth:		2",
  "MPLDriver*warningMessage*background:		Red",
  "MPLDriver*warningMessage*foreground:		White",
  /* Miscellaneous Application Specific */
  "mpl_motif.title:				MPL Outboard Driver",
  "mpl_motif*graphArea.topOffset:		0",
  "mpl_motif*graphArea.bottomOffset:	        4",
  "mpl_motif*graphArea.leftOffset:	        4",
  "mpl_motif*graphArea.rightOffset:	        4",
  "mpl_motif*graphArea.width:			750",
  "mpl_motif*graphArea.height:		        580",
  "mpl_motif*mainMenu*background:		SteelBlue3",
  "mpl_motif*statusBar*foreground:	        White",
  "mpl_motif*printText.columns:	 	        80",
  "mpl_motif.printCommand:			lpr",
  "mpl_motif.printToFile:			False",
  "mpl_motif.printFile:				mpl.ps",
  /* Postscript, MIF, or EPSF */
  "mpl_motif.printFileType:			Postscript",
  "mpl_motif.printLandscape:			True",
  "mpl_motif.printBorder:			False",
  "mpl_motif.printColor:			False",
#if defined(_WIN32)
  "mpl_motif.saveFile:				mpl.png",
  "mpl_motif.printFileBuffer:                   .mpl",
#else
  "mpl_motif.saveFile:				mpl.png",
#endif
  /* Postscript, MIF, or EPSF */
  "mpl_motif.saveFileType: 			PNG", 
  "mpl_motif.saveLandscape:			True",
  "mpl_motif.saveBorder:			True",
  "mpl_motif.saveColor:				True",
  "mpl_motif.dumpCommand:			xwd2ps -L | lpr -Papocp1",
  "mpl_motif.dumpFile:				mpl.xwd",
  "mpl_motif.dumpToFile:			False",
  "mpl_motif.confirmFile:			False",
  "mpl_motif.doublebuffer:			True",
  "mpl_motif.greyscale:				False",
  "mpl_motif.movie:				False",
  "mpl_motif.spectrum:				False",
  "mpl_motif.placementTopMargin:		85",
  "mpl_motif.placementBottomMargin:		45",
  "mpl_motif.placementLeftMargin:		20",
  "mpl_motif.placementRightMargin:		25",
  "mpl_motif.placementHorizontalSpacing:	20",
  "mpl_motif.placementVerticalSpacing:  	35",
  "mpl_motif.newZoom:                           FALSE",
  "mpl_motif.zoomFactor:                        0.2",
  "mpl_motif.geometry:                     700x541+200+150",
  /* My preferred line mapping for color monitors */
  "mpl_motif.greyscale: 	FALSE",
  "mpl_motif.bgColor:		Black",
  "mpl_motif.fgColor:		White",
  "mpl_motif.line0Color:	White",
  "mpl_motif.line1Color:	Red",
  "mpl_motif.line2Color:	DeepSkyBlue",
  "mpl_motif.line3Color:	Green",
  "mpl_motif.line4Color:	Yellow",
  "mpl_motif.line5Color:	Cyan",
  "mpl_motif.line6Color:	Orange",
  "mpl_motif.line7Color:	MediumSpringGreen",
  "mpl_motif.line8Color:	Gold",
  "mpl_motif.line9Color:	HotPink",
  "mpl_motif.line10Color:	Blue",
  "mpl_motif.line11Color:	LimeGreen",
  "mpl_motif.line12Color:	Tomato",
  "mpl_motif.line13Color:	Tan",
  "mpl_motif.line14Color:	Magenta",
  "mpl_motif.line15Color:	Grey75",
  /* Line-mapping for grey scale monitors */
#if 0
  "mpl_motif.greyscale: 	TRUE",
  "mpl_motif.bgColor:		Black",
  "mpl_motif.fgColor:		White",
  "mpl_motif.line0Color:	White",
  "mpl_motif.line1Color:	Grey95",
  "mpl_motif.line2Color:	Grey90",
  "mpl_motif.line3Color:	Grey85",
  "mpl_motif.line4Color:	Grey80",
  "mpl_motif.line5Color:	Grey75",
  "mpl_motif.line6Color:	Grey70",
  "mpl_motif.line7Color:	Grey65",
  "mpl_motif.line8Color:	Grey60",
  "mpl_motif.line9Color:	Grey55",
  "mpl_motif.line10Color:	Grey50",
  "mpl_motif.line11Color:	Grey45",
  "mpl_motif.line12Color:	Grey40",
  "mpl_motif.line13Color:	Grey35",
  "mpl_motif.line14Color:	Grey30",
  "mpl_motif.line15Color:	Grey25",
#endif
  /* Must end in NULL */
  NULL,
};

