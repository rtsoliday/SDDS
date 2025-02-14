/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "mpl_motif.h"
#include <io.h>
#if defined(__BORLANDC__)
#define _setmode(a,b) setmode(a,b)
#endif

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
  MSG msg;
  HACCEL hAccelTable;

  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance, IDC_MPLMOTIF, szWindowClass, MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  if (!InitInstance (hInstance, nCmdShow)) 
    {
      return FALSE;
    }

  hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_MPLMOTIF);

  while (GetMessage(&msg, NULL, 0, 0)) 
    {
      if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
	{
	  TranslateMessage(&msg);
	  DispatchMessage(&msg);
	}
    }

  return msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX); 

  wcex.style		= CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc	= (WNDPROC)WndProc;
  wcex.cbClsExtra	= 0;
  wcex.cbWndExtra	= 0;
  wcex.hInstance	= hInstance;
  wcex.hIcon		= LoadIcon(hInstance, (LPCTSTR)IDI_MPLMOTIF);
  wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground	= CreateSolidBrush(RGB(0,0,0));
  wcex.lpszMenuName	= (LPCSTR)IDC_MPLMOTIF;
  wcex.lpszClassName	= szWindowClass;
  wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

  return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
  long count=1;
  int i = 0;
  DEVMODE *dm;
  RECT rec;
  HANDLE handle;
  DWORD ret;
  hInst = hInstance;

  SystemParametersInfo(SPI_GETWORKAREA, 0, &rec, 0);
  if (W > rec.right)
    W = rec.right;
  if (H > rec.bottom)
    H = rec.bottom;

  display = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
			 CW_USEDEFAULT, 0, W, H, NULL, NULL, hInstance, NULL);

  if (!display)
    {
      return FALSE;
    }

  ShowWindow(display, nCmdShow);
  alloccolors();
  
  handle = GetStdHandle(STD_INPUT_HANDLE);
  if ((handle != INVALID_HANDLE_VALUE) && (handle != 0)) {
    ret = WaitForSingleObject(handle, 100);
    /* WAIT_FAILED is valid on WIN95 because 
       the handle is invalid for some reason */
    while (((ret == WAIT_OBJECT_0) || (ret == WAIT_FAILED)) 
	   && (count > 0)) {
      if (ret == WAIT_FAILED)
	Sleep(100);
      count = readdata();
      status(display);
      i++;
      if (i==1)
	UpdateWindow(display);
      ret = WaitForSingleObject(handle, 100);
    }
  }
  psDlg.lStructSize = sizeof(PAGESETUPDLG);
  psDlg.hwndOwner = display;
  psDlg.hInstance = (HINSTANCE)hInst;
  psDlg.lCustData = (LPARAM)NULL;
  psDlg.hPageSetupTemplate = (HGLOBAL)NULL;
  psDlg.Flags = PSD_ENABLEPAGEPAINTHOOK | PSD_INTHOUSANDTHSOFINCHES | PSD_RETURNDEFAULT;
  psDlg.lpfnPageSetupHook = (LPPAGESETUPHOOK)(FARPROC)NULL;
  psDlg.lpPageSetupTemplateName = (LPTSTR)NULL;
  psDlg.lpfnPagePaintHook = (LPPAGEPAINTHOOK)(FARPROC)PagePaintHook;
  PageSetupDlg(&psDlg);
  dm=(DEVMODE *)GlobalLock(psDlg.hDevMode);
  dm->dmOrientation = DMORIENT_LANDSCAPE;
  GlobalUnlock(psDlg.hDevMode);

  SetTimer(display,1,1000,NULL);

  return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int wmId, wmEvent;

  int wincount=0, check=0, displayed=0;
  static int nc_max=0;
  char *bufptr=NULL;
  HANDLE handle;
  DWORD ret;
  RECT rcClient;
  POINT ptClientUL;
  POINT ptClientLR;
//  static POINTS ptsBegin;
//  static POINTS ptsEnd;
  static POINTS ptsPrevEnd;
  static BOOL fPrevLine = FALSE;

  switch (message) 
    {
    case WM_COMMAND:
      wmId    = LOWORD(wParam); 
      wmEvent = HIWORD(wParam); 
      // Parse the menu selections:
      switch (wmId)
	{
	case IDM_ABOUT:
	  if (IsWindow(aboutBox)) {
	    EndDialog(aboutBox, LOWORD(wParam));
	    aboutBox = NULL;
          }
          aboutBox = CreateDialog(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), NULL, (DLGPROC) About);
          ShowWindow(aboutBox, SW_SHOW); 
	  break;
	case ID_CONTENTS:
	  if (IsWindow(contentsBox)) {
	    EndDialog(contentsBox, LOWORD(wParam));
	    contentsBox = NULL;
	  }
          contentsBox = CreateDialog(hInst, MAKEINTRESOURCE(IDD_CONTENTSBOX), NULL, (DLGPROC) Contents);
          ShowWindow(contentsBox, SW_SHOW); 
	  break;
	case ID_FILE_PRINTSETUP:
	  psDlg.lStructSize = sizeof(PAGESETUPDLG);
	  psDlg.hwndOwner = hWnd;
    
	  psDlg.hInstance = (HINSTANCE)hInst;
	  psDlg.lCustData = (LPARAM)NULL;
	  psDlg.hPageSetupTemplate = (HGLOBAL)NULL;
	  psDlg.Flags = PSD_MARGINS | PSD_ENABLEPAGEPAINTHOOK | PSD_INTHOUSANDTHSOFINCHES;
	  psDlg.lpfnPageSetupHook = (LPPAGESETUPHOOK)(FARPROC)NULL;
	  psDlg.lpPageSetupTemplateName = (LPTSTR)NULL;
	  psDlg.lpfnPagePaintHook = (LPPAGEPAINTHOOK)(FARPROC)PagePaintHook;

	  PageSetupDlg(&psDlg);
	  break;
	case ID_FILE_PRINT:
	  if (nplots)
	    printcb(hWnd);
	  break;
	case IDM_EXIT:
	  DestroyWindow(hWnd);
	  break;
	case ID_EXIT:
	  DestroyWindow(hWnd);
	  break;
	case ID_NAVIGATE_NEXT:
	  navigatecb(hWnd,1,0);
	  break;
	case ID_NAVIGATE_PREVIOUS:
	  navigatecb(hWnd,2,0);
	  break;
	case ID_NAVIGATE_DELETE:
	  navigatecb(hWnd,3,0);
	  break;
	case ID_NAVIGATE_TONUMBER:
	  if (IsWindow(navigateBox)) {
	    EndDialog(navigateBox, LOWORD(wParam));
	    navigateBox = NULL;
	  }
          navigateBox = CreateDialog(hInst, MAKEINTRESOURCE(IDD_NAVIGATE_NUMBER), NULL, (DLGPROC) NavigateNumber);
          ShowWindow(navigateBox, SW_SHOW); 
	  break;	  
	case ID_NAVIGATE_FIRST:
	  navigatecb(hWnd,4,1);
	  break;
	case ID_NAVIGATE_LAST:
	  if (last == (struct PLOTREC *)NULL) {
		Beep(500,50);
	  } else {
	    navigatecb(hWnd,4,last->nplot);
	  }
	  break;
	case ID_OPTIONS_PLACEMENT_FULLSCREEN:
	  placecb(hWnd,0);
	  break;
	case ID_OPTIONS_PLACEMENT_FIRSTQUARTER:
	case ID_OPTIONS_PLACEMENT_FIRSTQUARTER_F1:
	  placecb(hWnd,1);
	  break;
	case ID_OPTIONS_PLACEMENT_SECONDQUARTER:
	case ID_OPTIONS_PLACEMENT_SECONDQUARTER_F2:
	  placecb(hWnd,2);
	  break;
	case ID_OPTIONS_PLACEMENT_THIRDQUARTER:
	case ID_OPTIONS_PLACEMENT_THIRDQUARTER_F3:
	  placecb(hWnd,3);
	  break;
	case ID_OPTIONS_PLACEMENT_FOURTHQUARTER:
	case ID_OPTIONS_PLACEMENT_FOURTHQUARTER_F4:
	  placecb(hWnd,4);
	  break;
	case ID_OPTIONS_PLACEMENT_TOPHALF:
	  placecb(hWnd,5);
	  break;
	case ID_OPTIONS_PLACEMENT_BOTTOMHALF:
	  placecb(hWnd,6);
	  break;
	case ID_OPTIONS_PLACEMENT_LEFTHALF:
	  placecb(hWnd,7);
	  break;
	case ID_OPTIONS_PLACEMENT_RIGHTHALF:
	  placecb(hWnd,8);
	  break;
	case ID_OPTIONS_PLACEMENT_CENTER:
	  placecb(hWnd,9);
	  break;
	case ID_OPTIONS_GREYSCALE:
	  if (greyscale) {
	    CheckMenuItem(GetSubMenu(GetMenu(hWnd), 2), 3, MF_BYPOSITION | MF_UNCHECKED);
	    greyscale = 0;
	  } else {
	    CheckMenuItem(GetSubMenu(GetMenu(hWnd), 2), 3, MF_BYPOSITION | MF_CHECKED);
	    greyscale = 1;
	  }
	  RedrawWindow(hWnd,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
          status(hWnd);
	  break;
	case ID_OPTIONS_DASHES:
	  if (dashes) {
	    CheckMenuItem(GetSubMenu(GetMenu(hWnd), 2), 4, MF_BYPOSITION | MF_UNCHECKED);
	    dashes = 0;
	  } else {
	    CheckMenuItem(GetSubMenu(GetMenu(hWnd), 2), 4, MF_BYPOSITION | MF_CHECKED);
	    dashes = 1;
	  }
	  RedrawWindow(hWnd,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
          status(hWnd);
	  break;
	case ID_OPTIONS_DOUBLEBUFFER:
	  if (doublebuffer) {
	    CheckMenuItem(GetSubMenu(GetMenu(hWnd), 2), 5, MF_BYPOSITION | MF_UNCHECKED);
	    doublebuffer = 0;
	  } else {
	    CheckMenuItem(GetSubMenu(GetMenu(hWnd), 2), 5, MF_BYPOSITION | MF_CHECKED);
	    doublebuffer = 1;
	  }
	  break;
	case ID_OPTIONS_ZOOM_IN:
	  zoomcb(hWnd,1);
	  break;
	case ID_OPTIONS_ZOOM_OUT:
	  zoomcb(hWnd,2);
	  break;
	case ID_OPTIONS_ZOOM_UP:
	  zoomcb(hWnd,3);
	  break;
	case ID_OPTIONS_ZOOM_DOWN:
	  zoomcb(hWnd,4);
	  break;
	case ID_OPTIONS_ZOOM_LEFT:
	  zoomcb(hWnd,5);
	  break;
	case ID_OPTIONS_ZOOM_RIGHT:
	  zoomcb(hWnd,6);
	  break;
	case ID_OPTIONS_ZOOM_RESTORE:
	  zoomcb(hWnd,0);
	  break;
	case ID_OPTIONS_MOVIE:
	  movie(hWnd);
	  break;
	case ID_OPTIONS_MOUSETRACKER:
	  if (IsWindow(mouseTracker)) {
	    EndDialog(mouseTracker, LOWORD(wParam));
	    mouseTracker = NULL;
	  }
	  if(cur) {
          mouseTracker = CreateDialog(hInst, MAKEINTRESOURCE(IDD_MOUSETRACKER), hWnd, (DLGPROC) MouseTracker); 
          ShowWindow(mouseTracker, SW_SHOW);
	  }
	  break;	  
	case ID_OPTIONS_COLOR:
	  if (IsWindow(colorBox)) {
	    EndDialog(colorBox, LOWORD(wParam));
	    colorBox = NULL;
	  }
          colorBox = CreateDialog(hInst, MAKEINTRESOURCE(IDD_COLOR), NULL, (DLGPROC) ColorDialog);
          ShowWindow(colorBox, SW_SHOW); 
	  break;
	case ID_FILE_SAVEAS:
	  if (nplots)
	    savecb(hWnd);
	  break;
	default:
	  return DefWindowProc(hWnd, message, wParam, lParam);
	}
      break;
    case WM_PAINT:
      resize(hWnd);
	  fPrevLine = FALSE;
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_MOUSEMOVE:
	  if (IsWindow(mouseTracker)) {
	      HWND hEdit;
          char string[20];
		  hEdit = GetDlgItem(mouseTracker, IDC_EDIT1);
          sprintf(string,"%lf", MTRACKX(X0(GET_X_LPARAM(lParam))));
          SendMessage(hEdit, EM_SETSEL, 0, -1);
          SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)string);
          SendMessage(hEdit, EM_SCROLLCARET,0,0);
		  hEdit = GetDlgItem(mouseTracker, IDC_EDIT2);
          sprintf(string,"%lf", MTRACKY(Y0(GET_Y_LPARAM(lParam))));
          SendMessage(hEdit, EM_SETSEL, 0, -1);
          SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)string);
          SendMessage(hEdit, EM_SCROLLCARET,0,0);
	  }
	  if (wParam & MK_LBUTTON) {
		  HDC hdc;
	      hdc = GetDC(hWnd);
		  SetROP2(hdc, R2_NOTXORPEN);
		  if (fPrevLine) {
			  MoveToEx(hdc, ptsBegin.x, ptsBegin.y,
				  (LPPOINT) NULL);
			  LineTo(hdc, ptsPrevEnd.x, ptsBegin.y);
			  LineTo(hdc, ptsPrevEnd.x, ptsPrevEnd.y);
			  LineTo(hdc, ptsBegin.x, ptsPrevEnd.y);
			  LineTo(hdc, ptsBegin.x, ptsBegin.y);
		  }
		  ptsEnd = MAKEPOINTS(lParam);
		  MoveToEx(hdc, ptsBegin.x, ptsBegin.y, (LPPOINT) NULL);
		  LineTo(hdc, ptsEnd.x, ptsBegin.y);
		  LineTo(hdc, ptsEnd.x, ptsEnd.y);
		  LineTo(hdc, ptsBegin.x, ptsEnd.y);
		  LineTo(hdc, ptsBegin.x, ptsBegin.y);
		  fPrevLine = TRUE;
		  ptsPrevEnd = ptsEnd;
		  ReleaseDC(hWnd, hdc);
	  }
      break;
    case WM_LBUTTONDOWN:
		  HDC hdc;
	      hdc = GetDC(hWnd);
		  SetROP2(hdc, R2_NOTXORPEN);
		  if (fPrevLine) {
			  MoveToEx(hdc, ptsBegin.x, ptsBegin.y,
				  (LPPOINT) NULL);
			  LineTo(hdc, ptsPrevEnd.x, ptsBegin.y);
			  LineTo(hdc, ptsPrevEnd.x, ptsPrevEnd.y);
			  LineTo(hdc, ptsBegin.x, ptsPrevEnd.y);
			  LineTo(hdc, ptsBegin.x, ptsBegin.y);
			  fPrevLine = FALSE;
		  }
	  SetCapture(hWnd);
	  GetClientRect(hWnd, &rcClient);
	  ptClientUL.x = rcClient.left;
	  ptClientUL.y = rcClient.top;
	  ptClientLR.x = rcClient.right + 1;
	  ptClientUL.y = rcClient.bottom + 1;
	  ClientToScreen(hWnd, &ptClientUL);
	  ClientToScreen(hWnd, &ptClientLR);
	  SetRect(&rcClient, ptClientUL.x, ptClientUL.y,
		  ptClientLR.x, ptClientLR.y);
	  ptsBegin = MAKEPOINTS(lParam);
	  break;
    case WM_LBUTTONUP:
	  ReleaseCapture();
	  break;
    case WM_MBUTTONDOWN:
	  zoomcb(hWnd, 7);
	  break;
    case WM_RBUTTONDOWN:
	  zoomcb(hWnd, 0);
	  break;
    case WM_TIMER:
      handle = GetStdHandle(STD_INPUT_HANDLE);
      if ((handle != INVALID_HANDLE_VALUE) && (handle != 0)) {
	ret = WaitForSingleObject(handle, 100);
	/* WAIT_FAILED is valid on WIN95 because 
           the handle is invalid for some reason */
	while (((ret == WAIT_OBJECT_0) || (ret == WAIT_FAILED)) 
	       && (readdata() > 0)) {
	  status(display);
	  navigatecb(display,4,last->nplot);
	  ret = WaitForSingleObject(handle, 100);
	}
      }
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
  return 0;
}

void savecb(HWND hWnd) {
  OPENFILENAME ofn;
  static char szFilter[] = "PNG File (*.png)\0*.png\0Postscript File (*.ps)\0*.ps\0Encapsulated Postscript (*.eps)\0*.eps\0All Files(*.*)\0*.*\0\0";

  ZeroMemory(&ofn,sizeof(ofn));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = hWnd;
  ofn.hInstance = (HINSTANCE)hInst;
  ofn.lpstrFilter = szFilter;
  ofn.lpstrCustomFilter = NULL;
  ofn.nMaxCustFilter = 0;
  ofn.nFilterIndex = 1;
  ofn.lpstrFile = savefilename;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = MAX_PATH;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = NULL;
  ofn.nFileOffset = 0;
  ofn.nFileExtension = 0;
  ofn.lpstrDefExt = NULL;
  ofn.lCustData = 0;
  ofn.lpfnHook = (LPOFNHOOKPROC)SaveAsHookProc;
  ofn.lpTemplateName = MAKEINTRESOURCE(IDD_SAVEAS);
  ofn.Flags = OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_ENABLEHOOK|OFN_EXPLORER|OFN_ENABLETEMPLATE;
  if (GetSaveFileNamePreview(&ofn))
    {
      strcpy(savefilename,ofn.lpstrFile);
      if (lstrcmpi(ofn.lpstrFile + ofn.nFileExtension, _T("png")) == 0)
        pngprint();
      else if (lstrcmpi(ofn.lpstrFile + ofn.nFileExtension, _T("ps")) == 0) {
	savetype = 1;
        psprint();
      } else if (lstrcmpi(ofn.lpstrFile + ofn.nFileExtension, _T("eps")) == 0) {
	savetype = 2;
        psprint();
      } else
        DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, hWnd, (DLGPROC)ErrorDialog, IDS_UNKNOWNFILETYPE);
    }
}

BOOL CALLBACK SaveAsHookProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam) 
{ 
  static UINT uSaveOK;
  switch(uMessage)
    {
    case WM_INITDIALOG:
      {
	if (savelandscape)
	  CheckRadioButton(hWnd, IDC_LANDSCAPE, IDC_PORTRAIT, IDC_LANDSCAPE);
	else
	  CheckRadioButton(hWnd, IDC_LANDSCAPE, IDC_PORTRAIT, IDC_PORTRAIT);
	if (savecolor)
	  CheckRadioButton(hWnd, IDC_BLACKANDWHITE, IDC_COLOR, IDC_COLOR);
	else
	  CheckRadioButton(hWnd, IDC_BLACKANDWHITE, IDC_COLOR, IDC_BLACKANDWHITE);
	if (saveborder)
	  CheckDlgButton(hWnd, IDC_BORDER, BST_CHECKED);
	else
	  CheckDlgButton(hWnd, IDC_BORDER, BST_UNCHECKED);

	uSaveOK = RegisterWindowMessage(FILEOKSTRING);
      }
      break;
    case WM_DESTROY:
      break;
    default:
      if(uMessage == uSaveOK)
	{ 	  
	  if(IsDlgButtonChecked(hWnd, IDC_LANDSCAPE))
            savelandscape = 1;
	  else
            savelandscape = 0;
	  if(IsDlgButtonChecked(hWnd, IDC_COLOR))
            savecolor = 1;
	  else
            savecolor = 0;
	  if(IsDlgButtonChecked(hWnd, IDC_BORDER))
            saveborder = 1;
	  else
            saveborder = 0;
	}
      break;
    }
  return FALSE; 
} 



UINT CALLBACK PagePaintHook(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
  if (uiMsg == WM_PSD_GREEKTEXTRECT) 
    return TRUE;
  else if (uiMsg == WM_PSD_MARGINRECT) {
    COLORREF c[3];
    LPRECT rt = (LPRECT) lParam;
    H = rt->bottom - rt->top;
    W = rt->right - rt->left;
    scalex=((double)W-1.)/(double)XMAX;
    scaley=((double)H-1.)/(double)YMAX;
    offsetx=offsetx0=0;
    offsety=offsety0=0;
    c[0] = colors[0];
    c[1] = colors[1];
    c[2] = colors[2];
    colors[0] = white;
    colors[1] = colors[2] = black;
    displayplot((HDC) wParam,*rt,0);
    colors[0] = c[0];
    colors[1] = c[1];
    colors[2] = c[2];
    return FALSE;
  }
  return FALSE; 
}

// Mesage handler for error box.
LRESULT CALLBACK ErrorDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      HWND hEdit;
      TCHAR szMessage[MAX_LOADSTRING];
      LoadString(hInst, lParam, szMessage, MAX_LOADSTRING);
      hEdit = GetDlgItem(hDlg, IDC_EDIT1);
      SendMessage(hEdit, EM_SETSEL, 0, 0);
      SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)szMessage);
      SendMessage(hEdit, EM_SCROLLCARET,0,0);
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
	{
	  EndDialog(hDlg, LOWORD(wParam));
	  aboutBox = NULL;
	  return TRUE;
	}
      break;
    }
  return FALSE;
}

// Mesage handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
	{
	  EndDialog(hDlg, LOWORD(wParam));
	  aboutBox = NULL;
	  return TRUE;
	}
      break;
    }
  return FALSE;
}

// Mesage handler for about box.
LRESULT CALLBACK ColorDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  int n, imax, currentdoublebuffer = doublebuffer;

  switch (message)
    {
    case WM_INITDIALOG:
      return TRUE;

    case WM_COMMAND:
      int wmId;
      wmId = LOWORD(wParam);
      switch (wmId)
	{
        case IDOK:
	  EndDialog(hDlg, LOWORD(wParam));
	  colorBox = NULL;
	  return TRUE;
        case IDCANCEL:
	  EndDialog(hDlg, LOWORD(wParam));
	  colorBox = NULL;
	  return TRUE;
	case IDC_BLACKONWHITE:
          colors[0] = white;
	  for (n=1; n < NCOLORS; n++) 
            colors[n] = black;
	  RedrawWindow(display,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
          status(display);
	  return TRUE;
	case IDC_WHITEONBLACK:
          colors[0] = black;
	  for (n=1; n < NCOLORS; n++) 
            colors[n] = white;
	  RedrawWindow(display,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
          status(display);
	  return TRUE;
	case IDC_SPECTRUMLINES:
	  if(!spectrumallocated) allocspectrum();
	  if(grouplines) imax=NCOLORS;
	  else imax=linecolormax+1;
	  if(imax > 4) {
	    for(n=3; n < imax; n++) {
	      colors[n]=spectrum[((nspect-1)*(n-3))/(imax-4)];
	    }
	  }
	  RedrawWindow(display,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
          status(display);
	  return TRUE;	
	case IDC_GROUPLINES:
	  if (grouplines) {
	    grouplines = 0;
	  } else {
	    grouplines = 1;
	  }
	  return TRUE;
	}
      break;
    }
  return FALSE;
}

// Mesage handler for about box.
LRESULT CALLBACK NavigateNumber(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      HWND hEdit;
      char string[20];
      if (nplots) {
        hEdit = GetDlgItem(hDlg, IDC_EDIT1);
        sprintf(string,"%d",(cur->next)->nplot);
        SendMessage(hEdit, EM_SETSEL, 0, 0);
        SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)string);
        SendMessage(hEdit, EM_SCROLLCARET,0,0);
      }
      return TRUE;
    case WM_COMMAND:
      int wmId;
      wmId = LOWORD(wParam);
      switch (wmId)
	{
	case IDOK:
	  HWND hEdit;
	  char string[20];
	  hEdit = GetDlgItem(hDlg, IDC_EDIT1);
	  GetWindowText(hEdit, string, 10);
	    
	  EndDialog(hDlg, LOWORD(wParam));
	  navigatecb(display,4,atoi(string));
	  navigateBox = NULL;
	  return TRUE;
	case IDCANCEL:
	  EndDialog(hDlg, LOWORD(wParam));
	  navigateBox = NULL;
	  return TRUE;
	}
      break;
    }
  return FALSE;
}

// Mesage handler for about box.
LRESULT CALLBACK MouseTracker(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      return TRUE;
    case WM_COMMAND:
      int wmId;
      wmId = LOWORD(wParam);
      switch (wmId)
	{
	case IDOK:
	  EndDialog(hDlg, LOWORD(wParam));
	  mouseTracker = NULL;
	  return TRUE;
	}
      break;
    }
  return FALSE;
}

// Mesage handler for contents box.
LRESULT CALLBACK Contents(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      HWND hEdit;
      hEdit = GetDlgItem(hDlg, IDC_EDIT1);
      SendMessage(hEdit, EM_FMTLINES, TRUE, 0);
      SendMessage(hEdit, EM_SETSEL, 0, 0);
      SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)helptext);
      SendMessage(hEdit, EM_SCROLLCARET,0,0);
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wParam) == ID_CONTENTS_REMOVE || LOWORD(wParam) == IDCANCEL) 
	{
	  EndDialog(hDlg, LOWORD(wParam));
	  contentsBox = NULL;
	  return TRUE;
	}
      break;
    }
  return FALSE;
}

void printcb(HWND hWnd) {
  PRINTDLG pd;
  COLORREF c[3];
  DEVMODE *devm;
  DEVNAMES *devn;

  devm = (DEVMODE *)GlobalLock(psDlg.hDevMode);
  devn = (DEVNAMES *)GlobalLock(psDlg.hDevNames);
  GlobalUnlock(psDlg.hDevMode);
  GlobalUnlock(psDlg.hDevNames);

  ZeroMemory(&pd, sizeof(PRINTDLG));
  pd.lStructSize = sizeof(PRINTDLG);
  pd.hInstance = (HINSTANCE)hInst;
  pd.hwndOwner   = hWnd;
  pd.hDevMode    = devm;
  pd.hDevNames   = devn;
  pd.Flags       = PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDC | PD_NOSELECTION | PD_PAGENUMS; 
  pd.nCopies     = 1;
  pd.nFromPage   = cur->nplot; 
  pd.nToPage     = cur->nplot; 
  pd.nMinPage    = 1; 
  pd.nMaxPage    = nplots; 

  if (PrintDlg(&pd)==TRUE) {
    DOCINFO    di;
    RECT rt;
    struct PLOTREC *rec, *cur_old=cur;
    int found;
    int doub = doublebuffer;
    int page;
    DEVMODE *dm=(DEVMODE *)GlobalLock(pd.hDevMode);
    GlobalUnlock(pd.hDevMode);

    InitDocStruct( &di, "MyDoc");
    StartDoc( pd.hDC, &di );
   
    doublebuffer = 0;
    rt.top = (long)(dm->dmPrintQuality  * psDlg.rtMargin.top / 1000);;
    rt.left = (long)(dm->dmPrintQuality * psDlg.rtMargin.left / 1000);
    rt.right = GetDeviceCaps(pd.hDC,HORZRES)-(long)(dm->dmPrintQuality * psDlg.rtMargin.right / 1000);
    rt.bottom = GetDeviceCaps(pd.hDC,VERTRES)-(long)(dm->dmPrintQuality * psDlg.rtMargin.bottom / 1000);
    H = rt.bottom - rt.top;
    W = rt.right - rt.left;
    scalex=((double)W-1.)/(double)XMAX;
    scaley=((double)H-1.)/(double)YMAX;
    offsetx=offsetx0=0;
    offsety=offsety0=0;
    c[0] = colors[0];
    c[1] = colors[1];
    c[2] = colors[2];
    colors[0] = white;
    colors[1] = colors[2] = black;
    linewidthscale = 6;
    if (dm->dmColor == DMCOLOR_MONOCHROME) {
      greyscale = 1;
    }
    if (!(pd.Flags&PD_PAGENUMS)) {
      pd.nFromPage = pd.nMinPage;
      pd.nToPage = pd.nMaxPage;
    }
    for (page=pd.nFromPage;page<=pd.nToPage;page++) {
      rec=last;
      found = 0;
      while(rec) {
        if(rec->nplot == page) {
	  found=1;
	  break;
	}
        rec=rec->prev;
      }
      if(found) {
	cur=rec;
        StartPage( pd.hDC );
        displayplot(pd.hDC,rt,1);
        EndPage( pd.hDC );
      }
    }
    greyscale = 0;
    linewidthscale = 1;
    colors[0] = c[0];
    colors[1] = c[1];
    colors[2] = c[2];
    doublebuffer = doub;
    EndDoc( pd.hDC );
    DeleteDC(pd.hDC);
    cur=cur_old;
  }
}

/*===============================*/
/* Initialize DOCINFO structure  */
/* ==============================*/
void InitDocStruct( DOCINFO* di, char* docname)
{
  // Always zero it before using it.
  memset( di, 0, sizeof( DOCINFO ) );
  // Fill in the required members.
  di->cbSize = sizeof( DOCINFO );
  di->lpszDocName = docname;
}

/**************************** makeplotrec *********************************/
struct PLOTREC *makeplotrec(void)
  /* Makes a new plot record, which is also the last */
{
  struct PLOTREC *plotrec,*rec;
  int nkept;

  plotrec=(struct PLOTREC *)calloc(1,sizeof(struct PLOTREC));
  if(!plotrec) {
  }
  else {
    nplots++;
    plotrec->nplot=nplots;
    plotrec->nc=0;
    plotrec->prev=last;
    plotrec->next=plotrec;
    plotrec->buffer=(char *)NULL;
    if(last) last->next=plotrec;
    last=plotrec;
  }
  /* Check for keep limit */
  if(keep > 0) {
    rec=last;
    nkept=0;
    while(rec) {
      nkept++;
      if(nkept > keep) {
	destroyplotrec(rec);
	break;
      }
      rec=rec->prev;
    }
  }
  /* Return */    
  return plotrec;
}

/**************************** status **************************************/
void status(HWND hWnd)
{
  char string[MAX_LOADSTRING];
  
  if(cur) {
    sprintf(string,"OAG WIN32 Plotter -- Plot %d of %d",cur->nplot,nplots);
  }
  else {
    sprintf(string,"OAG WIN32 Plotter -- No plots");
    SetWindowText(hWnd,(LPCTSTR)string);
  }
  SetWindowText(hWnd,(LPCTSTR)string);
}

/**************************** quit ****************************************/
void quit(void)
{
  exit(0);
}

/**************************** destroyplotrec ******************************/
void destroyplotrec(struct PLOTREC *rec)
  /* Destroys the specified plot record and adjusts cur if necessary */
{
  /* Check if it is NULL */
  if(!rec) return;
  /* Unlink from the list */
  if(rec->prev == (struct PLOTREC *)NULL) {     /* First in the list */
    if(rec == last) {     /* Also last */
      cur=last=(struct PLOTREC *)NULL;
    }
    else {
      (rec->next)->prev=(struct PLOTREC *)NULL;
      if(rec == cur) cur=rec->next;
    }
  }
  else if(rec == last) {     /* Last in the list */
    if(rec->prev == (struct PLOTREC *)NULL) {     /* Also first */
      cur=last=(struct PLOTREC *)NULL;
    }
    else {
      last=(rec->prev)->next=rec->prev;
      if(rec == cur) cur=last;
    }
  }
  else {
    (rec->prev)->next=rec->next;
    (rec->next)->prev=rec->prev;
    if(cur == rec) cur=rec->next;
  }
  /* Free the commands buffer */
  if(rec->buffer) free(rec->buffer);
  /* Free the structure */
  free(rec);
  if(rec == curwrite) curwrite=(struct PLOTREC *)NULL;
  rec=(struct PLOTREC *)NULL;
}

/**************************** resize ****************************************/
void resize(HWND hWnd)
{
  RECT graphareawindow;
  PAINTSTRUCT ps;
  HDC hdc;

  /* Get window geometry */
  GetClientRect(hWnd,&graphareawindow);
  H=graphareawindow.bottom;
  W=graphareawindow.right;
  if (!norestore)
    restore();
  norestore = 0;
  hdc = BeginPaint(hWnd, &ps);
  displayplot(hdc,graphareawindow,0);
  EndPaint(hWnd, &ps);
}
/**************************** restore ***************************************/
void restore(void)
{
  scalex=((double)W-1.)/(double)XMAX;
  scaley=((double)H-1.)/(double)YMAX;
  offsetx0=offsety0=0.;
  offsetx=offsetx0-.5;
  offsety=offsety0-.5;
  x0old=0.;
  y0old=0.;
  x1old=XMAX;
  y1old=YMAX;
  deltax=x1old-x0old;
  deltay=y1old-y0old;
  /* DEBUG */
  /*     printf("restore: W=%d H=%d scalex=%g offsetx0=%g scaley=%g offsety0=%g deltax=%g deltay=%g\n", */
  /*       W,H,scalex,offsetx0,scaley,offsety0,deltax,deltay); */
}

/**************************** displayplot *********************************/
void displayplot(HDC hdc, RECT rt, int screen)
  /* Draws the plot according to the commands in the buffer */
  /* Draws to a pixmap and then copies it to the window            */
  /* Without double buffer draws directly to the window as well    */
{
  VTYPE x,y,lt;
  int n,type;
  char *bufptr,command;
  unsigned short r, g, b;
  HDC HDCDoubleBuffer;
  HBITMAP DoubleBuffer;
  LOGBRUSH lb;
  HBRUSH hbr;
  HPEN hpen;
  RECT rt2;
  DWORD userstyle[2][18][8] = {{{1,0,1,0,1,0,1,0}, {1,6,1,6,1,6,1,6}, 
    {1,0,1,0,1,0,1,0}, {2,2,2,2,2,2,2,2},
      {6,2,6,2,6,2,6,2}, {6,6,6,6,6,6,6,6},
	{2,6,2,6,2,6,2,6}, {2,2,2,6,2,2,2,6},
	  {6,2,6,6,6,2,6,6}, {2,2,6,2,2,2,6,2},
	    {2,6,6,6,2,6,6,6}, {2,2,6,6,2,2,6,6},
	      {6,2,2,6,6,2,2,6}, {1,2,1,2,1,6,0,0},
		{1,2,1,2,6,2,0,0}, {1,2,1,2,6,6,0,0}, 
		  {6,2,1,2,1,6,0,0}, {6,2,1,2,6,6,0,0}},
    {{1,0,1,0,1,0,1,0}, {6,36,6,36,6,36,6,36}, 
      {1,0,1,0,1,0,1,0}, {12,12,12,12,12,12,12,12}, 
	{36,12,36,12,36,12,36,12}, {36,36,36,36,36,36,36,36}, 
	  {12,36,12,36,12,36,12,36}, {12,12,12,36,12,12,12,36}, 
	    {36,12,36,36,36,12,36,36}, {12,12,36,12,12,12,36,12}, 
	      {12,36,36,36,12,36,36,36}, {12,12,36,36,12,12,36,36}, 
		{36,12,12,36,36,12,12,36}, {6,12,6,12,6,36,0,0}, 
		  {6,12,6,12,36,12,0,0}, {6,12,6,12,36,36,0,0}, 
		    {36,12,6,12,6,36,0,0}, {36,12,6,12,36,36,0,0}}};
  /*  rt.top = 0;
      rt.left = 0;
      rt.right = GetDeviceCaps(hdc,HORZRES);
      rt.bottom = GetDeviceCaps(hdc,VERTRES);
  */
  currentbrushcolor = colors[0];
  currentcolor = colors[1];
  currentuserstyle = 0;
  /* Create double buffer */
  hbr = CreateSolidBrush(colors[0]);
  if (doublebuffer) {
    HDCDoubleBuffer=CreateCompatibleDC(hdc);
    DoubleBuffer=CreateCompatibleBitmap(hdc, W, H);

    DeleteObject(SelectObject(HDCDoubleBuffer, DoubleBuffer));
    rt2.left = 0;
    rt2.top = 0;
    rt2.right = W;
    rt2.bottom = H;
    FillRect(HDCDoubleBuffer, &rt2, hbr);
    SetBkMode(HDCDoubleBuffer, TRANSPARENT);
  } else {
    FillRect(hdc, &rt, hbr);
    SetBkMode(hdc, TRANSPARENT);
  }
  DeleteObject(hbr);
  /* Loop over accumulated commands from inboard driver */
  if(!cur) {
    if (doublebuffer) {
      BitBlt(hdc, rt.left, rt.top, W, H, HDCDoubleBuffer, 0, 0, SRCCOPY);
      DeleteObject(DoubleBuffer);
      DeleteObject(HDCDoubleBuffer);
    }
    return;
  }

  bufptr=cur->buffer;
  for(n=0; n < cur->nc; ) {
    switch (command=*bufptr++) {
    case 'V':    /*   X11_vector(x,y) - draw vector  */
      memcpy((char *)&x,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&y,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      if (doublebuffer) {
	MoveToEx(HDCDoubleBuffer, X(cx), Y(cy), NULL);
	LineTo(HDCDoubleBuffer, X(x), Y(y));
      } else {
	MoveToEx(hdc, rt.left + X(cx), rt.top + Y(cy), NULL);
	LineTo(hdc, rt.left + X(x), rt.top + Y(y));
      }
      cx = x; cy = y;
      break;
    case 'M':    /*   X11_move(x,y) - move  */
      memcpy((char *)&cx,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&cy,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      break;
    case 'P':	/* X11_dot(x, y) -- dot */
      memcpy((char *)&cx,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&cy,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      if (doublebuffer) {
	MoveToEx(HDCDoubleBuffer, X(cx), Y(cy), NULL);
	LineTo(HDCDoubleBuffer, X(cx)+1, Y(cy)+1);
      } else {
	MoveToEx(hdc, rt.left + X(cx), rt.top + Y(cy), NULL);
	LineTo(hdc, rt.left + X(cx)+1, rt.top + Y(cy)+1);
      }
      cx++;
      cy++;
      break;
    case 'L':	/*   X11_linetype(type) - set line type  */
      memcpy((char *)&lt,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+sizeof(VTYPE);
      lt = lt%16;
      lt+=2;     /* Convert to color index */ 
      if(lt >= NCOLORS) lt=NCOLORS-1;
      if(lt > linecolormax) linecolormax=lt;
      type = PS_SOLID;

      currentlinewidth = 1 * linewidthscale;
      if (greyscale) {
        lb.lbColor = colors[1];
        lb.lbStyle = BS_SOLID;
        if (doublebuffer) {
          hpen = (HPEN)SelectObject(HDCDoubleBuffer, ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 
								  currentlinewidth, &lb, 8, userstyle[screen][lt]));
	} else {
          hpen = (HPEN)SelectObject(hdc, ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 
						      currentlinewidth, &lb, 8, userstyle[screen][lt]));
	}
	currentuserstyle = lt;
        currentcolor = colors[1];
      } else if (dashes) {
        lb.lbColor = colors[lt];
        lb.lbStyle = BS_SOLID;
        if (doublebuffer) {
          hpen = (HPEN)SelectObject(HDCDoubleBuffer, ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 
								  currentlinewidth, &lb, 8, userstyle[screen][lt]));
	} else {
          hpen = (HPEN)SelectObject(hdc, ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 
						      currentlinewidth, &lb, 8, userstyle[screen][lt]));
	}
	currentuserstyle = lt;
        currentcolor = colors[lt];
      } else {
        if (doublebuffer) {
	  hpen = (HPEN)SelectObject(HDCDoubleBuffer, CreatePen(type, currentlinewidth, colors[lt]));
	} else {
	  hpen = (HPEN)SelectObject(hdc, CreatePen(type, currentlinewidth, colors[lt]));
	}
        currentcolor = colors[lt];
      }
      DeleteObject(hpen);
      break;
    case 'W':	/*   X11_line_thickness(thickness) - set line width  */
      memcpy((char *)&lt,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+sizeof(VTYPE);
      currentlinewidth = lt * linewidthscale;
      if ((greyscale) || (dashes)) {
        lb.lbColor = currentcolor;
        lb.lbStyle = BS_SOLID;
        if (doublebuffer) {
          hpen = (HPEN)SelectObject(HDCDoubleBuffer, ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 
								  currentlinewidth, &lb, 8, userstyle[screen][currentuserstyle]));
	} else {
          hpen = (HPEN)SelectObject(hdc, ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 
						      currentlinewidth, &lb, 8, userstyle[screen][currentuserstyle]));
	}
      } else {
        if (doublebuffer)
	  hpen = (HPEN)SelectObject(HDCDoubleBuffer, CreatePen(PS_SOLID, currentlinewidth, currentcolor));
        else
	  hpen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, currentlinewidth, currentcolor));
      }
      DeleteObject(hpen);
      break;
    case 'B':	/* Fill Box */
      {
	VTYPE shade, xl, xh, yl, yh;
	RECT rec;
	memcpy((char *)&shade,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&xl,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&xh,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&yl,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&yh,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	n+=sizeof(char)+5*sizeof(VTYPE);
	shade = shade%nspect;
	rec.left = rt.left + X(xh);
	rec.top = rt.top + Y(yh);
	rec.right = rt.left + X(xl);
	rec.bottom = rt.top + Y(yl);
	if(!spectrumallocated) allocspectrum();
	hbr = CreateSolidBrush(spectrum[shade]);
	if (doublebuffer)
	  FillRect(HDCDoubleBuffer, &rec, hbr);
	else
	  FillRect(hdc, &rec, hbr);
        DeleteObject(hbr);
      }
      break;
    case 'U':    /*   X11_user(ax,bx,ay,by) - user coordinate scaling  */
      memcpy((char *)&userax,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&userbx,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&useray,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&userby,bufptr,sizeof(double)); bufptr+=sizeof(double);
      n+=sizeof(char)+4*sizeof(double);
      break;
    case 'G':
    case 'R':
    case 'E':
      n+=sizeof(char);
      break;
    case 'C':
      {
	memcpy((char *)&r,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&g,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&b,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	n+=sizeof(char)+3*sizeof(VTYPE);
	/*r = r%65536;*/
	currentcolor = RGB(round((255.0/65536.0)*r),round((255.0/65536.0)*g),round((255.0/65536.0)*b));
	if (doublebuffer)
	  hpen = (HPEN)SelectObject(HDCDoubleBuffer, CreatePen(PS_SOLID, currentlinewidth, currentcolor));
	else
	  hpen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, currentlinewidth, currentcolor));
        DeleteObject(hpen);
      }
      break;
    case 'S':   /* Allocate spectral spectrum */
      {
	VTYPE num, spec, r0, g0, b0, r1, g1, b1;
	memcpy((char *)&num,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&spec,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&r0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&g0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&b0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&r1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&g1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&b1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	n+=sizeof(char)+8*sizeof(VTYPE);
	nspect = num;
	spectral = spec;
	if (spectral == 0) {
	  customspectral = 1;
	  red0=r0;
	  green0=g0;
	  blue0=b0;
	  red1=r1;
	  green1=g1;
	  blue1=b1;
	} else {
	  customspectral = 0;
	}
	allocspectrum();
      }
      break;
    default:
      DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_INVALIDDRAWCOMMAND);
      break;
    }	
  }     /* End for loop */
  /* Copy the pixmap to the window */
  if (doublebuffer) {
    BitBlt(hdc, rt.left, rt.top, W, H, HDCDoubleBuffer, 0, 0, SRCCOPY);
    DeleteObject(DoubleBuffer);
    DeleteObject(HDCDoubleBuffer);
  }
}

/**************************** alloccolors *********************************/
int alloccolors(void)
{
  int n;
  /* Allocate black */
  black=RGB(0,0,0); 
  /* Allocate white */
  white=RGB(255,255,255); 
  /* Allocate option colors */

  colors[0] = black;
  colors[1] = colors[2] = white;
  colors[3] = RGB(255,0,0); 
  colors[4] = RGB(0,0,255);
  colors[5] = RGB(0,255,0);
  colors[6] = RGB(255,255,0);
  colors[7] = RGB(255,0,255);
  colors[8] = RGB(0,255,255);
  colors[9] = RGB(50,205,50);
  colors[10] = RGB(255,215,0);
  colors[11] = RGB(255,165,0);
  colors[12] = RGB(255,105,180);
  colors[13] = RGB(0,191,255);
  colors[14] = RGB(0,250,154);
  colors[15] = RGB(255,99,71);
  colors[16] = RGB(210,180,140);
  colors[17] = RGB(128,128,128);
  for(n=0; n < NCOLORS; n++) {
    colors_orig[n]=colors[n];
    colorsalloc[n]=1;
  }
  currentcolor = white;
  return 1;
}

int allocspectrum(void)
{
  int n,ok=1,windowmessage0=windowmessage, k;
  double hue;
  BYTE red, green, blue;

  /* Allocate spectrum colors */
  windowmessage=0;
  for (n=0; n < nspect; n++) {
    spectrumalloc[n]=1;
    if (spectral) {
      hue = 5 * n / (nspect-1.0);
      if (hue < 1) {
	red = (BYTE)(255);
	green = (BYTE)(hue*256);
	blue = (BYTE)(0);
      } else if ((hue >= 1) && (hue < 2)) {
	if (hue==1) hue=1.00001;
	red = (BYTE)((2-hue)*256);
	green = (BYTE)(255);
	blue = (BYTE)(0);
      } else if ((hue >= 2) && (hue < 3)) {
	red = (BYTE)(0);
	green = (BYTE)(255);
	blue = (BYTE)((hue-2)*256);
      } else if ((hue >= 3) && (hue < 4)) {
	if (hue==3) hue=3.00001;
	red = (BYTE)(0);
	green = (BYTE)((4-hue)*256);
	blue = (BYTE)(255);
      } else {
	if (hue>=5) hue=4.99999;
	red = (BYTE)((hue-4)*256);
	green = (BYTE)(0);
	blue = (BYTE)(255);
      }
      spectrum[n] = RGB(red,green,blue);
    } else if (customspectral) {
      hue = n / (nspect-1.0);
      red = (BYTE)((red0 + hue * (red1 - red0))/256.0);
      green = (BYTE)((green0 + hue * (green1 - green0))/256.0);
      blue = (BYTE)((blue0 + hue * (blue1 - blue0))/256.0);
      spectrum[n] = RGB(red,green,blue);
    } else {
      k = 0;
      for (n=0;n<nspect;n++) {
	if (k < 256) {
	  spectrum[n] = RGB((BYTE)(0),
			    (BYTE)(k),
			    (BYTE)(255));
	} else if ((k >= 256) && (k < 512)) {
	  spectrum[n] = RGB((BYTE)(0),
			    (BYTE)(255),
			    (BYTE)(511-k));
	} else if ((k >= 512) && (k < 768)) {
	  spectrum[n] = RGB((BYTE)(k-512),
			    (BYTE)(255),
			    (BYTE)(0));
	} else if ((k >= 768) && (k < 1024)) {
	  spectrum[n] = RGB((BYTE)(255),
			    (BYTE)(1023-k),
			    (BYTE)(0));
	} else {
	  spectrum[n] = RGB((BYTE)(255),
			    (BYTE)(0),
			    (BYTE)(k-1024));
	}
	k += 1279.0 / (nspect - 1);
      }
    }
  }


  windowmessage=windowmessage0;
  spectrumallocated=1;
  return ok;
}

long readdata(void) {
  int numread, wincount=0, numvals, step, check=0, newnc, savepipe=0;
  static int nc_max=0;
  char command;
  char *bufptr=NULL;
  long count=0;
  DWORD length;

  _setmode( _fileno( stdin ), _O_BINARY );

  if (savepipe) {
    FILE *file;
	
    file=fopen("mpl_motif.out","wb");
    while (1) {
      numread = fread(&command,sizeof(char),1,stdin);
      if (numread != 1)
	break;
      count++;
      fwrite(&command,sizeof(char),1,file);
    }
    if (ferror(stdin)) fprintf(stderr,"Error reading input\n");
    if (count)
      fclose(file);
    return count;
  }
  Sleep(20);
  if (PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), (LPVOID)NULL, (DWORD)0, 
		    (LPDWORD)NULL, &length, (LPDWORD)NULL) != TRUE) {
    return 0;
  }
  if (length < 1) {
    return 0;
  }
  while(1) {
    numread=fread(&command,sizeof(char),1,stdin);
    if (numread != 1) {
      break;
    }
    count++;
    wincount = 1;
    if (command == 'G') {     /* Enter graphics mode */
      /* Trim any old buffer since we are through writing it */
      if (curwrite) {
	curwrite->buffer=(char *)realloc(curwrite->buffer,
					 curwrite->nc);
      }
      /* Make a new plot */
      curwrite=makeplotrec();
      nc_max=0;
      /* If there is no cur, set it to curwrite, else show old movie frame */
      if(!cur) {
	cur=curwrite;
      }
      bufptr=curwrite->buffer;
    }
    else if (!curwrite) continue;  /* No record, probably deleted */
    else if(command == 'E') {      /* Leave graphics mode */
      /* Trim the buffer since we are (probably) through writing it */
      curwrite->buffer=(char *)realloc(curwrite->buffer,
				       curwrite->nc);
      nc_max=curwrite->nc;
      break;
    }
    else if(command == 'R') {     /* Exit */
      quit();
    }
    /* Determine the space required in addition to commands themselves */
    switch(command) {
    case 'V':
    case 'M':
    case 'P':
      numvals=2;
      break;
    case 'L':
    case 'W':
      numvals=1;
      break;
    case 'B':
      numvals=5;
      break;
    case 'U':
      numvals=4*sizeof(double)/sizeof(VTYPE);
      break;
    case 'G':
    case 'R':
    case 'E':
      numvals=0;
      break;
    case 'C':
      numvals=3;
      break;
    case 'S':
      numvals=8;
      break;
    default:
      exit(1);
      break;
    }
    /* Allocate more buffer space if necessary */
    step=numvals*sizeof(VTYPE);
    newnc=curwrite->nc+sizeof(char)+step;
    if(newnc >= nc_max) {
      /* (Re)allocate */
      curwrite->buffer=(char *)realloc(curwrite->buffer,
				       (nc_max+=DNC));
      bufptr=curwrite->buffer+curwrite->nc;
    }         
    /* Store the commands */
    *bufptr=command;
    bufptr+=sizeof(char);
    curwrite->nc++;
    if(numvals) {
      fread(bufptr,sizeof(VTYPE)*numvals,1,stdin);
      bufptr+=step;
      curwrite->nc+=step;
    }
  }
  /* Check for errors */
  if (wincount) {
    if(check) DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_OUTOFLOOP);
    if(ferror(stdin)) DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_ERRORREADINGINPUT);
  }
  return count;
}

/**************************** navigatecb **********************************/
void navigatecb(HWND hWnd, int clientdata, int page)
{
  int n, currentdoublebuffer = doublebuffer;
  int errmsg=0;
  n=(int)clientdata;
  switch(n) {
  case 1:
    if(cur == (struct PLOTREC *)NULL) {
      Beep(500,50);
    }
    else if(cur == last) {
      Beep(500,50);
    }
    else {
      cur=cur->next;
      RedrawWindow(hWnd,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
      status(hWnd);
    }
    break;
  case 2:
    if(cur == (struct PLOTREC *)NULL) {
      Beep(500,50);
    }
    else if(cur->prev == (struct PLOTREC *)NULL) {
      Beep(500,50);
    }
    else {
      cur=cur->prev;
      RedrawWindow(hWnd,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
      status(hWnd);
    }
    break;
  case 3:
    if(cur == (struct PLOTREC *)NULL) {
      Beep(500,50);
    }
    else {
      destroyplotrec(cur);
      RedrawWindow(hWnd,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
      status(hWnd);
    }
    break;
  case 4:
    {
      struct PLOTREC *rec=last;
      int found = 0;

      while(rec) {
	if(rec->nplot == page) {
	  found=1;
	  break;
	}
	rec=rec->prev;
      }
      if(found) {
	cur=rec;
	RedrawWindow(hWnd,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	status(hWnd);
      }
    }
    break;
  }
}

/**************************** placecb *************************************/
void placecb(HWND hWnd, int clientdata)
{
  RECT rec;

  SystemParametersInfo(SPI_GETWORKAREA, 0, &rec, 0);
  switch(clientdata) {
  case 0:
    SetWindowPos(hWnd, HWND_TOP, 
		 rec.left, rec.top, rec.right - rec.left, rec.bottom - rec.top, 
		 SWP_SHOWWINDOW);
    break;
  case 1:
    SetWindowPos(hWnd, HWND_TOP, 
		 rec.left, rec.top, (rec.right - rec.left)/2, (rec.bottom - rec.top)/2, 
		 SWP_SHOWWINDOW);
    break;
  case 2:
    SetWindowPos(hWnd, HWND_TOP, 
		 (rec.right - rec.left)/2, rec.top, (rec.right - rec.left)/2, (rec.bottom - rec.top)/2, 
		 SWP_SHOWWINDOW);
    break;
  case 3:
    SetWindowPos(hWnd, HWND_TOP, 
		 rec.left, (rec.bottom - rec.top)/2, (rec.right - rec.left)/2, (rec.bottom - rec.top)/2, 
		 SWP_SHOWWINDOW);
    break;
  case 4:
    SetWindowPos(hWnd, HWND_TOP, 
		 (rec.right - rec.left)/2, (rec.bottom - rec.top)/2, (rec.right - rec.left)/2, (rec.bottom - rec.top)/2, 
		 SWP_SHOWWINDOW);
    break;
  case 5:
    SetWindowPos(hWnd, HWND_TOP, 
		 rec.left, rec.top, rec.right - rec.left, (rec.bottom - rec.top)/2, 
		 SWP_SHOWWINDOW);
    break;
  case 6:
    SetWindowPos(hWnd, HWND_TOP, 
		 rec.left, (rec.bottom - rec.top)/2, rec.right - rec.left, (rec.bottom - rec.top)/2, 
		 SWP_SHOWWINDOW);
    break;
  case 7:
    SetWindowPos(hWnd, HWND_TOP, 
		 rec.left, rec.top, (rec.right - rec.left)/2, rec.bottom - rec.top, 
		 SWP_SHOWWINDOW);
    break;
  case 8:
    SetWindowPos(hWnd, HWND_TOP, 
		 (rec.right - rec.left)/2, rec.top, (rec.right - rec.left)/2, rec.bottom - rec.top, 
		 SWP_SHOWWINDOW);
    break;
  case 9:
    SetWindowPos(hWnd, HWND_TOP, 
		 (int)((rec.right - rec.left)*.125), (int)((rec.bottom - rec.top)*.125), 
		 (int)((rec.right - rec.left)*.75), (int)((rec.bottom - rec.top)*.75), 
		 SWP_SHOWWINDOW);
    break;
  }
}

void zoomcb(HWND hWnd, int clientdata)
{
  int currentdoublebuffer = doublebuffer;
  switch(clientdata) {
  case 0:
    restore();
    break;
  case 1:
  case 2:
    {
      double x0,y0,x1,y1,fac=.5*(3-2*clientdata);
      x0=x0old+fac*(1.-ZOOMFAC)*deltax;
      x1=x1old-fac*(1.-ZOOMFAC)*deltax;
      y0=y0old+fac*(1.-ZOOMFAC)*deltay;
      y1=y1old-fac*(1.-ZOOMFAC)*deltay;
      zoom(x0,y0,x1,y1);
    }
    break;
  case 3:
    offsety0-=MOVEFAC*deltay*scaley;
    offsety=offsety0-.5;
    y0old+=MOVEFAC*deltay;
    y1old+=MOVEFAC*deltay;
    break;
  case 4:
    offsety0+=MOVEFAC*deltay*scaley;
    offsety=offsety0-.5;
    y0old-=MOVEFAC*deltay;
    y1old-=MOVEFAC*deltay;
    break;
  case 5:
    offsetx0-=MOVEFAC*deltax*scalex;
    offsetx=offsetx0-.5;
    x0old-=MOVEFAC*deltax;
    x1old-=MOVEFAC*deltax;
    break;
  case 6:
    offsetx0+=MOVEFAC*deltax*scalex;
    offsetx=offsetx0-.5;
    x0old+=MOVEFAC*deltax;
    x1old+=MOVEFAC*deltax;
    break;
  case 7:
	double x0,y0,x1,y1;
	if (ptsBegin.x < ptsEnd.x) {
		x0 = ptsBegin.x;
		x1 = ptsEnd.x;
	} else {
		x0 = ptsEnd.x;
		x1 = ptsBegin.x;
	}
	if (ptsBegin.y > ptsEnd.y) {
		y0 = ptsBegin.y;
		y1 = ptsEnd.y;
	} else {
		y0 = ptsEnd.y;
		y1 = ptsBegin.y;
	}
    zoom(X0(x0),Y0(y0),X0(x1),Y0(y1));
    break;

  }
  norestore = 1;
  RedrawWindow(hWnd,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
  status(hWnd);
}

/**************************** zoom ****************************************/
void zoom(double x0, double y0, double x1, double y1)
{
  int x0p=X(x0),y0p=Y(y0),x1p=X(x1),y1p=Y(y1);

  /* Check for zero-length box */
  if ((x0p == x1p) || (y0p == y1p)) {
    return;
  }
  /* x */
  x0old=x0;
  x1old=x1;
  deltax=x1-x0;
  scalex=((double)W-1.)/deltax;
  offsetx0=scalex*x0;
  offsetx=offsetx0-.5;
  /* y */
  y0old=y0;
  y1old=y1;
  deltay=y1-y0;
  scaley=((double)H-1.)/deltay;
  offsety0=scaley*(YMAX-y1);
  offsety=offsety0-.5;
}

/**************************** movie ***************************************/
void movie(HWND hWnd)
{
  struct PLOTREC *first,*rec;
  int currentdoublebuffer = doublebuffer;
  /* Check if there are plots */    
  if(cur == (struct PLOTREC *)NULL) {
    return;
  }
  /* Find the first plot */    
  rec=first=cur;
  while(rec) {
    first=rec;
    rec=rec->prev;
  }
  /* Display all the plots */
  rec=first;
  while(1) {
    cur=rec;
    RedrawWindow(hWnd,NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    status(hWnd);
    if(rec == last) break;
    rec=rec->next;
  }
}

/**************************** pngprint ************************************/
void pngprint()
  /* Makes PNG from the buffer */
{
  double convertx,converty,offx,offy;
  VTYPE x,y,lt;
  int n;
  int endflag=0;
  int iborder,icolor,ilandscape;
  char *ifilename;
  char tempname[BUFSIZ];
  char *bufptr,command;
  unsigned short r, g, b;
  FILE *outfile = NULL;

#define CX(x) (int) ((double)(x)*convertx+offx)
#define CY(y) (int) ((double)(y)*converty+offy)

  /* Set switches */
  iborder=saveborder;
  icolor=savecolor;
  ilandscape=savelandscape;
  ifilename=savefilename;
  /* Open file or pipe */

  sprintf(tempname, "%s", ifilename);
  outfile=fopen(tempname,"wb");
  if(outfile == NULL) {
    DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_UNABLESAVEIMAGE);
    return;
  }

  /* Initialize */
  if(ilandscape) {
    convertx=(double)PNG_XMAX/(double)X11_XMAX;
    converty=(double)PNG_YMAX/(double)X11_YMAX;
    offx=.5;
    offy=.5;
    PNG_init();
  }
  else {     /* Currently the same as landscape */
    convertx=(double)PNG_XMAX/(double)X11_XMAX;
    converty=(double)PNG_YMAX/(double)X11_YMAX;
    offx=.5;
    offy=.5;
    PNG_init();
  }
  /* Loop over accumulated commands from inboard driver */
  bufptr=cur->buffer;
  for(n=0; n < cur->nc; ) {
    switch (command=*bufptr++) {
    case 'V':    /*   X11_vector(x,y) - draw vector  */
      memcpy((char *)&x,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&y,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      PNG_vector(CX(x),CY(y));
      break;
    case 'M':    /*   X11_move(x,y) - move  */
      memcpy((char *)&x,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&y,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      PNG_move(CX(x),CY(y));
      break;
    case 'P':	/* X11_dot(x, y) -- dot */
      memcpy((char *)&x,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&y,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      PNG_dot(CX(x),CY(y),0);
      break;
    case 'L':	/*   X11_linetype(type) - set line type  */
      memcpy((char *)&lt,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+sizeof(VTYPE);
      PNG_linetype(lt);
      break;
    case 'W':	/*   X11_line_thickness(thickness) - set line width  */
      memcpy((char *)&lt,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+sizeof(VTYPE);
      /*PNG_line_thickness(lt);*/
      break;
    case 'B':	/* Fill Box */
      {
	VTYPE shade, xl, xh, yl, yh;
		
	memcpy((char *)&shade,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&xl,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&xh,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&yl,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&yh,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	n+=sizeof(char)+5*sizeof(VTYPE);
	shade = shade%nspect;
	PNG_fill_box(shade,CX((int)xl),CX((int)xh),CY((int)yl),CY((int)yh));
      }
      break;
    case 'U':    /*   X11_user(ax,bx,ay,by) - user coordinate scaling  */
      memcpy((char *)&userax,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&userbx,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&useray,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&userby,bufptr,sizeof(double)); bufptr+=sizeof(double);
      n+=sizeof(char)+4*sizeof(double);
      break;
    case 'G':
      n+=sizeof(char);
      PNG_graphics();
      if(iborder) {
	PNG_move(0,0);
	PNG_vector(PNG_XMAX-1,0);
	PNG_vector(PNG_XMAX-1,PNG_YMAX-1);
	PNG_vector(0,PNG_YMAX-1);
	PNG_vector(0,0);
      }
      break;
    case 'E':
      n+=sizeof(char);
      endflag=1;
      PNG_text(outfile);
      break;
    case 'R':
      n+=sizeof(char);
      break;
    case 'C':	/*   X11_linetype(type) - set line type  */
      memcpy((char *)&r,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&g,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&b,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+3*sizeof(VTYPE);
      if(icolor)
	PNG_color(r,g,b);
     break;
    case 'S':	/* Allocate spectral spectrum */
      {
	VTYPE num, spec, r0, g0, b0, r1, g1, b1;
	memcpy((char *)&num,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&spec,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&r0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&g0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&b0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&r1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&g1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&b1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	n+=sizeof(char)+8*sizeof(VTYPE);
	nspect = num;
	spectral = spec;
	if (spectral == 0) {
	  customspectral = 1;
	  red0=r0;
	  green0=g0;
	  blue0=b0;
	  red1=r1;
	  green1=g1;
	  blue1=b1;
	} else {
	  customspectral = 0;
	}
	PNG_spectral(num, spec, r0, g0, b0, r1, g1, b1);
      }
      break;
    default:
      DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_INVALIDDRAWCOMMAND);
      break;
    }	
  }     /* End for loop */
  /* Terminate */
  if(!endflag) {
    PNG_text(outfile);
    PNG_reset();
  }


  fclose(outfile);
  if(ferror(outfile)) {
    DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_FILEORPIPEERROR);
  }

#undef CX
#undef CY
}

int PNG_reset(void)
  /* end graphing altogether */
{
  return 0;
}

int PNG_text(FILE *outfile)
  /* end current graph page */
{
  /*All operating systems will use png instead of png in the future*/
  gdImagePng(pngImage, outfile);
  fflush(outfile);
  gdImageDestroy(pngImage);
  return 0;
}

int PNG_graphics(void)
{
  int i;
  pngImage = gdImageCreateTrueColor(png_xmax, png_ymax);
  for (i=0;i<NCOLORS;i++) {
    pngLinetype[i] = gdImageColorAllocate(pngImage, GetRValue(colors[i]), 
					  GetGValue(colors[i]), GetBValue(colors[i]));
  }
  png_linetype = 0;
  if (png_color_index != 0) {
    png_colorList = (int*)malloc(sizeof(*png_colorList)*png_color_index);
    for (i=0;i<png_color_index;i++) {
      png_colorList[i] = gdImageColorAllocate(pngImage, png_colors[i].red / 256, png_colors[i].green / 256, png_colors[i].blue / 256);
    }
  }
  /* gdImageFilledRectangle(pngImage, 0, 0, png_xmax - 1, png_ymax - 1, pngBlack);*/

  return 0;
}

int PNG_fill_box(int shade, int xl, int xh, int yl, int yh)
{
  return 0;
}
int PNG_color(unsigned short red, unsigned short green, unsigned short blue)
{
  return 0;
}
int PNG_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1)
{
  return 0;
}
int CPS_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1)
{
  return 0;
}
int PNG_linetype(int linetype)
{
  if ((png_color_index == 0) || (linetype < 1000)) {
    png_linetype = linetype%16 +2;
  } else {
    png_linetype = linetype;
  }
  return 0;
}

int PNG_dot(int x, int y, int number)
{
  y = png_ylast - y;
  if ((png_color_index == 0) || (png_linetype < 1000)) {
    gdImageSetPixel(pngImage, x, y, pngLinetype[png_linetype]);
  } else {
    gdImageSetPixel(pngImage, x, y, png_colorList[(png_linetype - 1000)%png_color_index]);
  }
  png_xposition = x;
  png_yposition = y;
  return 0;
}

int PNG_move(int x, int y)
{
  y = png_ylast - y;
  png_xposition = x;
  png_yposition = y;
  return 0;
}

int PNG_vector(int x, int y)
{
  y = png_ylast - y;
  if ((png_color_index == 0) || (png_linetype < 1000)) {
    gdImageLine(pngImage, png_xposition, png_yposition, x, y, pngLinetype[png_linetype]);
  } else {
    gdImageLine(pngImage, png_xposition, png_yposition, x, y, png_colorList[(png_linetype - 1000)%png_color_index]);
  }
  png_xposition = x;
  png_yposition = y;
  return 0;
}

int PNG_init(void)
{
  png_xlast = (png_xmax = PNG_XMAX) - 1;
  png_ylast = (png_ymax = PNG_YMAX) - 1;
  return 0;
}

/**************************** psprint *************************************/
void psprint()
  /* Makes postscript from the buffer */
{
  double convertx,converty,offx,offy;
  VTYPE x,y,lt;
  int n;
  int endflag=0;
  int ifile,iborder,icolor,ilandscape,itype;
  char *ifilename;
  char tempname[BUFSIZ];
  char *bufptr,command;
  unsigned short r, g, b;
  FILE *outfile = NULL;

#define CX(x) (int) ((double)(x)*convertx+offx)
#define CY(y) (int) ((double)(y)*converty+offy)

  //    XDefineCursor(display,mainwindow,watch);
  //    XFlush(display);
  //    if(printpopup) readprinttext();
  /* Set switches */
  ifile=1;
  iborder=saveborder;
  icolor=savecolor;
  ilandscape=savelandscape;
  itype=savetype;
  ifilename=savefilename;

  /* Open file or pipe */
  sprintf(tempname, "%s", ifilename);
  outfile=fopen(tempname,"w");
  if(outfile == NULL) {
    DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_UNABLESAVEIMAGE);
    return;
  }
  /* Initialize */
  if(itype == 1) {     /* PS */
    if(ilandscape) {
      convertx=(double)PS_XMAX/(double)X11_XMAX;
      converty=(double)PS_YMAX/(double)X11_YMAX;
      offx=.5;
      offy=.5;
      if(!icolor) PS1_init(outfile);
      else CPS1_init(outfile);
    }
    else {
      convertx=(double)PS_XMAX/(double)X11_XMAX;
      converty=(double)PS_YMAX/(double)X11_YMAX;
      offx=.5;
      offy=.5;
      if(!icolor) PS1_init_rot(outfile);
      else CPS1_init_rot(outfile);
    }
  }
  else {
    if(ilandscape) {
      convertx=(double)PS_XMAX/(double)X11_XMAX;
      converty=(double)PS_YMAX/(double)X11_YMAX;
      offx=.5;
      offy=.5;
    }
    else {     /* Currently same as landscape */
      convertx=(double)PS_XMAX/(double)X11_XMAX;
      converty=(double)PS_YMAX/(double)X11_YMAX;
      offx=.5;
      offy=.5;
    }
  }
  /* Loop over accumulated commands from inboard driver */
  bufptr=cur->buffer;
  for(n=0; n < cur->nc; ) {
    switch (command=*bufptr++) {
    case 'V':    /*   X11_vector(x,y) - draw vector  */
      memcpy((char *)&x,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&y,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      PS_vector(outfile, CX(x),CY(y));
      break;
    case 'M':    /*   X11_move(x,y) - move  */
      memcpy((char *)&x,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&y,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      PS_move(outfile, CX(x),CY(y));
      break;
    case 'P':	/* X11_dot(x, y) -- dot */
      memcpy((char *)&x,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&y,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+2*sizeof(VTYPE);
      PS_dot(outfile, CX(x),CY(y),0);
      break;
    case 'L':	/*   X11_linetype(type) - set line type  */
      memcpy((char *)&lt,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+sizeof(VTYPE);
      if(!icolor)
	PS_linetype(outfile, lt);
      else
	CPS_linetype(outfile, lt);
      break;
    case 'W':	/*   X11_line_thickness(thickness) - set line thickness  */
      memcpy((char *)&lt,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+sizeof(VTYPE);
      PS_line_thickness(outfile, lt);
      break;
    case 'B':	/* Fill Box */
      {
	VTYPE shade, xl, xh, yl, yh;
		
	memcpy((char *)&shade,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&xl,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&xh,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&yl,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&yh,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	n+=sizeof(char)+5*sizeof(VTYPE);
	shade = shade%nspect;
	if(!icolor) PS_fill_box(outfile, shade,CX((int)xl),CX((int)xh),CY((int)yl),CY((int)yh));
	/**/	        else CPS_fill_box(outfile, shade,CX((int)xl),CX((int)xh),CY((int)yl),CY((int)yh));
		
      }
      break;
    case 'U':    /*   X11_user(ax,bx,ay,by) - user coordinate scaling  */
      memcpy((char *)&userax,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&userbx,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&useray,bufptr,sizeof(double)); bufptr+=sizeof(double);
      memcpy((char *)&userby,bufptr,sizeof(double)); bufptr+=sizeof(double);
      n+=sizeof(char)+4*sizeof(double);
      break;
    case 'G':
      n+=sizeof(char);
      if(itype == 1) {     /* PS */
	if(!icolor) PS_graphics(outfile);
	else CPS_graphics(outfile);
      }
      else {     /* EPSF */
	if(!icolor) EPSF_graphics(outfile);
	else CEPSF_graphics(outfile);
      }
      if(iborder) {
	PS_move(outfile, 0,0);
	PS_vector(outfile, PS_XMAX,0);
	PS_vector(outfile, PS_XMAX,PS_YMAX);
	PS_vector(outfile, 0,PS_YMAX);
	PS_vector(outfile, 0,0);
      }
      break;
    case 'E':
      n+=sizeof(char);
      endflag=1;
      if(itype == 1) PS_text(outfile);
      else EPSF_text(outfile);
      break;
    case 'R':
      n+=sizeof(char);
      break;
    case 'C':	/*   X11_linetype(type) - set line type  */
      memcpy((char *)&r,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&g,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      memcpy((char *)&b,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
      n+=sizeof(char)+3*sizeof(VTYPE);
      if(icolor)
	CPS_color(outfile, r,g,b);
      break;
    case 'S':	/* Allocate spectral spectrum */
      {
	VTYPE num, spec, r0, g0, b0, r1, g1, b1;
	memcpy((char *)&num,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&spec,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&r0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&g0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&b0,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&r1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&g1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	memcpy((char *)&b1,bufptr,sizeof(VTYPE)); bufptr+=sizeof(VTYPE);
	n+=sizeof(char)+8*sizeof(VTYPE);
	nspect = num;
	spectral = spec;
	if (spectral == 0) {
	  customspectral = 1;
	  red0=r0;
	  green0=g0;
	  blue0=b0;
	  red1=r1;
	  green1=g1;
	  blue1=b1;
	} else {
	  customspectral = 0;
	}
	CPS_spectral(num, spec, r0, g0, b0, r1, g1, b1);
      }
      break;
    default:
      DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_INVALIDDRAWCOMMAND);
      break;
    }	
  }     /* End for loop */
  /* Terminate */
  if(!endflag) {
    if(itype == 1) {     /* PS */
      PS_text(outfile);
      PS_reset(outfile);
    }
    else {     /* EPSF */
      EPSF_text(outfile);
      EPSF_reset();
    }
  }
  if(ifile) fclose(outfile);
  if(ferror(outfile)) {
    DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_FILEORPIPEERROR);
  }

#undef CX
#undef CY
}

int EPSF_graphics(FILE *outfile)
{
  int i;
  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: mpl/sddsplot\n");
  fprintf(outfile, "%%%%DocumentFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%DocumentNeededFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%BoundingBox: 0 0 %d %d\n", 
          (int)(1*(PS_XMAX)/PS_SC+0.5),  
          (int)(1*(PS_YMAX)/PS_SC+0.5) );
  fprintf(outfile, "%%%%EndComments\n");
  fprintf(outfile, "40 dict begin\n");
  fprintf(outfile, "/gnulinewidth %.3f def\n", PS_LW);
  fprintf(outfile, "/vshift %d def\n", -PS_VCHAR1/3);
  fprintf(outfile, "/dl {%d mul} def\n", PS_SC); /* dash length */
  fprintf(outfile, "/hpt %.1f def\n", PS_HTIC/2.0);
  fprintf(outfile, "/vpt %.1f def\n", PS_VTIC/2.0);
  for ( i=0; PS_header[i] != NULL; i++)
    fprintf(outfile, "%s", PS_header[i]);
  for ( i=0; PS_BW_def[i] != NULL; i++)
    fprintf(outfile, "%s", PS_BW_def[i]);
  fprintf(outfile, "%%%%EndProlog\n");
  fprintf(outfile, "%%%%BeginSetup\n");
  fprintf(outfile, "/Gnu_save save def\n");
  fprintf(outfile, "%.3f %.3f scale\n", 1.0/PS_SC, 1.0/PS_SC);
  fprintf(outfile, "0 setgray\n0 setlinecap\n");
  fprintf(outfile, "%%%%IncludeFont: %s\n",  "Courier");
  fprintf(outfile, "/%s findfont %d ",  "Courier", PS_VCHAR1 );
  fprintf(outfile, "scalefont setfont\n");
  fprintf(outfile, "newpath\n");
  ps_path_count = 0;
  fprintf(outfile, "%%%%EndSetup\n");
  reset_PS_lastlinetypes();
  return 0;
}


int CEPSF_graphics(FILE *outfile)
{
  int i;
  char *line = "0123456789abcdefgh";

  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: mpl/sddsplot\n");
  fprintf(outfile, "%%%%DocumentFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%DocumentNeededFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%BoundingBox: 0 0 %d %d\n", 
	  (int)(1*(PS_XMAX)/PS_SC+0.5),  
	  (int)(1*(PS_YMAX)/PS_SC+0.5) );
  fprintf(outfile, "%%%%EndComments\n");
  fprintf(outfile, "40 dict begin\n");
  fprintf(outfile, "/gnulinewidth %.3f def\n", PS_LW);
  fprintf(outfile, "/vshift %d def\n", -PS_VCHAR1/3);
  fprintf(outfile, "/dl {%d mul} def\n", PS_SC); /* dash length */
  fprintf(outfile, "/hpt %.1f def\n", PS_HTIC/2.0);
  fprintf(outfile, "/vpt %.1f def\n", PS_VTIC/2.0);
  for ( i=0; PS_header[i] != NULL; i++)
    fprintf(outfile, "%s", PS_header[i]);
  for ( i=0; PS_color_def[i] != NULL; i++)
    fprintf(outfile, "%s", PS_color_def[i]);

  fprintf(outfile, "/LT0 { PL 1 1 1 setrgbcolor } def\n");
  fprintf(outfile, "/LT1 { PL 0 0 0 setrgbcolor } def\n");
  fprintf(outfile, "/LT2 { PL 0 0 0 setrgbcolor } def\n");
  for ( i=3; i<NCOLORS; i++) {

    fprintf(outfile, "/LT%c { PL %.2f %.2f %.2f setrgbcolor } def\n", line[i], 
	    GetRValue(colors[i])/255.0, 
	    GetGValue(colors[i])/255.0, 
	    GetBValue(colors[i])/255.0);
  }
  fprintf(outfile, "%%%%EndProlog\n");
  fprintf(outfile, "%%%%BeginSetup\n");
  fprintf(outfile, "/Gnu_save save def\n");
  fprintf(outfile, "%.3f %.3f scale\n", 1.0/PS_SC, 1.0/PS_SC);
  fprintf(outfile, "0 setgray\n0 setlinecap\n");
  fprintf(outfile, "%%%%IncludeFont: %s\n",  "Courier");
  fprintf(outfile, "/%s findfont %d ",  "Courier", PS_VCHAR1 );
  fprintf(outfile, "scalefont setfont\n");
  fprintf(outfile, "newpath\n");
  ps_path_count = 0;
  fprintf(outfile, "%%%%EndSetup\n");
  reset_PS_lastlinetypes();
  doCPSExtraSetups(outfile);
  return 0;
}


int EPSF_text(FILE *outfile)
{
  fprintf(outfile, "stroke\nGnu_save restore\n");
  fprintf(outfile, "showpage\n");
  ps_path_count = 0;
  fprintf(outfile, "%%%%Trailer\n");
  fprintf(outfile, "end\n");
  return 0;
}


int EPSF_reset(void)
{
  return 0;
}


int PS1_init(FILE *outfile)
{
  PS_init(outfile);
  reset_PS_lastlinetypes();
  return 0;
}

int CPS1_init(FILE *outfile)
{
  PS_ProcessDeviceArgs();
  CPS_init(outfile);
  reset_PS_lastlinetypes();
  return 0;
}

int PS1_init_rot(FILE *outfile)
{
  PS_init_rot(outfile);
  reset_PS_lastlinetypes();
  return 0;
}


int CPS1_init_rot(FILE *outfile)
{
  PS_ProcessDeviceArgs();
  CPS_init_rot(outfile);
  reset_PS_lastlinetypes();
  return 0;
}

int PS_init(FILE *outfile)
{
  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: mpl/sddsplot\n");
  fprintf(outfile, "%%%%DocumentFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%DocumentNeededFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%Pages: (atend)\n");
  fprintf(outfile, "%%%%BoundingBox: %d 50 550 %d\n", 
	  (int)(550.5-1*(PS_YMAX)/PS_SC*2),  
	  (int)(50.5+1*(PS_XMAX)/PS_SC*2) );
  fprintf(outfile, "%%%%EndComments\n");
  fprintf(outfile, "/GnuTransform {\n");
  fprintf(outfile, "  90 rotate\n  50 -550 translate\n  2 2 scale\n");
  fprintf(outfile, "} def\n");
  fprintf(outfile, "%%%%EndProlog\n");
  pspage = 0;
  reset_PS_lastlinetypes();
  return 0;
}

int CPS_init(FILE *outfile)
{
  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: mpl/sddsplot\n");
  fprintf(outfile, "%%%%DocumentFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%DocumentNeededFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%Pages: (atend)\n");
  fprintf(outfile, "%%%%BoundingBox: %d 50 550 %d\n", 
	  (int)(550.5-1*(PS_YMAX)/PS_SC*2),  
	  (int)(50.5+1*(PS_XMAX)/PS_SC*2) );
  fprintf(outfile, "%%%%EndComments\n");
  fprintf(outfile, "/GnuTransform {\n");
  fprintf(outfile, "  90 rotate\n  90 -550 translate\n  1.7 2.0 scale\n");
  fprintf(outfile, "} def\n");
  fprintf(outfile, "%%%%EndProlog\n");
  pspage = 0;
  reset_PS_lastlinetypes();
  return 0;
}

/* rotated post-script output */
int PS_init_rot(FILE *outfile)
{
  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: mpl/sddsplot\n");
  fprintf(outfile, "%%%%DocumentFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%DocumentNeededFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%Pages: (atend)\n");
  fprintf(outfile, "%%%%BoundingBox: %d 50 550 %d\n", 
	  (int)(550.5-1*(PS_YMAX)/PS_SC*2),  
	  (int)(50.5+1*(PS_XMAX)/PS_SC*2) );
  fprintf(outfile, "%%%%EndComments\n");
  fprintf(outfile, "/GnuTransform {\n");
  fprintf(outfile, "  45 90 translate\n  1.45 2.45 scale\n");
  fprintf(outfile, "} def\n");
  fprintf(outfile, "%%%%EndProlog\n");
  pspage = 0;
  reset_PS_lastlinetypes();
  return 0;
}


/* rotated post-script output */
int CPS_init_rot(FILE *outfile)
{
  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: mpl/sddsplot\n");
  fprintf(outfile, "%%%%DocumentFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%DocumentNeededFonts: %s\n",  "Courier");
  fprintf(outfile, "%%%%Pages: (atend)\n");
  fprintf(outfile, "%%%%BoundingBox: %d 50 550 %d\n", 
	  (int)(550.5-1*(PS_YMAX)/PS_SC*2),  
	  (int)(50.5+1*(PS_XMAX)/PS_SC*2) );
  fprintf(outfile, "%%%%EndComments\n");
  fprintf(outfile, "/GnuTransform {\n");
  fprintf(outfile, "  45 90 translate\n  1.45 2.45 scale\n");
  fprintf(outfile, "} def\n");
  fprintf(outfile, "%%%%EndProlog\n");
  pspage = 0;
  reset_PS_lastlinetypes();
  return 0;
}



int PS_graphics(FILE *outfile)
{
  fprintf(outfile, "%%%%Page: ? %d\n", ++pspage);
  fprintf(outfile, "gsave\nGnuTransform\n");
  fprintf(outfile, "%%%%BeginDocument: EPSF\n");
  fprintf(outfile, "1 dict begin\n/showpage {} def\n");
  EPSF_graphics(outfile);
  /*        reset_PS_lastlinetypes(); */
  return 0;
}


int CPS_graphics(FILE *outfile)
{
  fprintf(outfile, "%%%%Page: ? %d\n", ++pspage);
  fprintf(outfile, "gsave\nGnuTransform\n");
  fprintf(outfile, "%%%%BeginDocument: EPSF\n");
  fprintf(outfile, "1 dict begin\n/showpage {} def\n");
  CEPSF_graphics(outfile);
  /*        reset_PS_lastlinetypes(); */
  return 0;
}


int PS_text(FILE *outfile)
{
  EPSF_text(outfile);
  fprintf(outfile, "%%%%EndDocument\n");
  fprintf(outfile, "end\nshowpage\ngrestore\n");
  return 0;
}



int PS_reset(FILE *outfile)
{
  fprintf(outfile, "%%%%Trailer\n");
  fprintf(outfile, "%%%%Pages: %d\n", pspage);
  return 0;
}



int PS_line_thickness(FILE *outfile, int line_thickness)
{
  fprintf(outfile, "LW%d\n", line_thickness);
  force_linetype=1;
  return 0;
}


int PS_linetype(FILE *outfile, int linetype)
{
  char *line = "0123456789abcdefgh"; 
  linetype = linetype%16 + 2;
  if ((!force_linetype) && (linetype==last_PS_linetype))
    return 0;
  fprintf(outfile, "LT%c\n",  line[linetype]);
  ps_path_count = 0;
  last_PS_linetype = linetype;
  force_linetype = 0;
  return 0;
}

int CPS_linetype(FILE *outfile, int linetype)
{
  char *line = "0123456789abcdefgh";
  if ((cps_color_index == 0) || (linetype < 1000)) {
    linetype = linetype%16 + 2;
    if ((!force_linetype) && (linetype==last_CPS_linetype))
      return 0;
    fprintf(outfile, "LT%c\n",  line[linetype]);
    force_linetype = 0;
  } else {
    linetype -= 1000;
    linetype = linetype%cps_color_index;
    CPS_color(outfile, cps_colors[linetype].red, cps_colors[linetype].green, cps_colors[linetype].blue);
    force_linetype = 1;
  }
  ps_path_count = 0;
  last_CPS_linetype = linetype;
  return 0;
}


int reset_PS_lastlinetypes()
{
  last_CPS_linetype = last_PS_linetype = -1;
  return 0;
}

int PS_move(FILE *outfile, int x, int y)
{
  fprintf(outfile, "%d %d M\n",  x,  y);
  ps_path_count += 1;
  return 0;
}


int PS_vector(FILE *outfile, int x, int y)
{
  fprintf(outfile, "%d %d L\n",  x,  y);
  ps_path_count += 1;
  if (ps_path_count >= 400) {
    fprintf(outfile, "currentpoint stroke moveto\n");
    ps_path_count = 0;
  }
  return 0;
}

/* postscript dot routine */
int PS_dot(FILE *outfile, int x, int y, int number)
{
  fprintf(outfile, "%d %d P\n",  x,  y);
  ps_path_count = 0;
  return 0;
}

int PS_fill_box(FILE *outfile, int shade, int xl, int xh, int yl, int yh)
{
  if (ps_path_count) {
    fprintf(outfile, "currentpoint stroke moveto\n");
    ps_path_count = 0;
  }
  shade = 100-shade%101;
  fprintf(outfile,  "%d %d %d %d %.2f RF\n", 
	  xl,  yl,  xh-xl,  yh-yl,  shade/100.0);
  last_PS_linetype = -1;
  return 0;
}



int CPS_fill_box(FILE *outfile, int shade, int xl, int xh, int yl, int yh)
{
  double r,  g,  b;

  if (ps_path_count) {
    fprintf(outfile, "currentpoint stroke moveto\n");
    ps_path_count = 0;
  }
  shade = shade%101;
  RGB_values(&r,  &g,  &b,  shade/100.0);
        
  fprintf(outfile,  "%d %d %d %d %.3f %.3f %.3f RF\n", 
	  xl,  yl,  xh-xl,  yh-yl,  r,  g,  b);
  last_CPS_linetype = -1;
  return 0;
}

int CPS_color(FILE *outfile, unsigned short red, unsigned short green, unsigned short blue)
{
  fprintf(outfile, "PL %.3f %.3f %.3f setrgbcolor\n", (red/65535.0), (green/65535.0), (blue/65535.0));
  force_linetype=1;
  return 0;
}

int CPS_add_color(unsigned short red, unsigned short green, unsigned short blue)
{
  if (cps_color_index >= cps_color_maxindex) {
    cps_color_maxindex += 100;
    if (cps_color_index == 0) {
      cps_colors = (MotifColor *)malloc(sizeof(*cps_colors)*cps_color_maxindex);
    } else {
      cps_colors = (MotifColor *)realloc(cps_colors, sizeof(*cps_colors)*(++cps_color_maxindex));
    }
  }
  cps_colors[cps_color_index].red = red;
  cps_colors[cps_color_index].green = green;
  cps_colors[cps_color_index].blue = blue;
  cps_color_index++;
  return 0;
}

/* the following is based on an algorithm from Ken Evans: */
void RGB_values(double *red, double *green, double *blue, double hue)
{
  long ngroups = 5,  nmembers = 45,  ntotal;
  double gh,  ih;
  double high=1,  medium=0.375,  deltax,  hx,  r,  g,  b;

  ntotal = ngroups*nmembers;
  deltax = (high-medium)/nmembers;
  hx = hue*ntotal;
  if (hx<0)
    hx = 0;
  if (hx>ntotal)
    hx = ntotal;
  gh = (int)(hx/nmembers);
  ih = (int)(hx);
  switch ((int)gh) {
  case 0: 
    r = medium;
    g = medium + (ih-gh*nmembers)*deltax;
    b = high;
    break;
  case 1:
    r = medium;
    g = high;
    b = high-(ih-gh*nmembers)*deltax;
    break;
  case 2: 
    r = medium + (ih-gh*nmembers)*deltax;
    g = high;
    b = medium;
    break;
  case 3: 
    r = high;
    g = high-(ih-gh*nmembers)*deltax;
    b = medium;
    break;
  case 4: 
    r = high;
    g = medium;
    b = medium+(ih-gh*nmembers)*deltax;
    break;
  default:
    r = high;
    g = medium;
    b = high;
    break;
  }
    

  *red = r;
  *green = g;
  *blue = b;
}

int PS_ProcessDeviceArgs()
{
  char s[100];
  unsigned long flags;
  flags = CPS_ONWHITE_BG;
  if (flags&CPS_ONBLACK_BG) {
    /* insert a "definition" that will fill in the plot with black */
    if (!(CPS_extra_setups = (char**)malloc(sizeof(*CPS_extra_setups)*2))) {
      fprintf(stderr, "memory allocation failure in PS_ProcessDeviceArgs\n");
      exit(1);
    }
    sprintf(s,  "0 0 %d %d 0 0 0 RF\n",
            PS_XLAST, PS_YLAST);
    CPS_extra_setups[0] = (char *)malloc(sizeof(char)*(strlen(s)+1));
    strcpy(CPS_extra_setups[0], s);
    CPS_extra_setups[1] = NULL;
  }
  return 0;
}

int doCPSExtraSetups(FILE *outfile)
{
  long i;
  if (!CPS_extra_setups)
    return 0;
  for (i=0; CPS_extra_setups[i] != NULL; i++)
    fprintf(outfile, "%s", CPS_extra_setups[i]);
  return 0;
}

