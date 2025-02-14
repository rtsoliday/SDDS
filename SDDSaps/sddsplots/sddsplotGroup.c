/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "mdb.h"
#include "graph.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>


static char *groupby_usage = "-groupby=[=request][,tag][,fileIndex][,nameIndex][,page][subpage][,fileString][,nameString][,inameString]";

#if (GROUPBY_FILEINDEX!=0 || GROUPBY_PAGE!=1 || GROUPBY_NAMEINDEX!=2 || GROUPBY_REQUEST!=3 || GROUPBY_TAG!=4 || GROUPBY_FILESTRING!=5 || GROUPBY_NAMESTRING!=6 || GROUPBY_SUBPAGE!=7 || GROUPBY_INAMESTRING!=8)
#error "Inconsistent GROUBY_X number/array assignments"
#endif
static char *groupbyName[GROUPING_NAMES] = {
  "fileindex", "page", "nameindex", "request", "tag", "filestring", "namestring", "subpage",
  "inamestring",
} ;

void swapRanks(long rank, long code, GROUPBY_SETTINGS *settings);
void determine_panel_equivalent_requests(PLOT_SPEC *plspec);


long groupby_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    long iItem, code;
    GROUPBY_SETTINGS *settings;

    if (items==0 || items>4)
        return bombre("invalid -groupby syntax", groupby_usage, 0);
    settings = &plotspec->groupBy;
    settings->groupbyGiven = 1;
    settings->ranksChanged = 0;
    settings->rank[0] = GROUPBY_REQUEST;
    settings->rank[1] = GROUPBY_TAG;
    settings->rank[2] = GROUPBY_FILEINDEX;
    settings->rank[3] = GROUPBY_NAMEINDEX;
    settings->rank[4] = GROUPBY_PAGE;
    settings->rank[5] = GROUPBY_SUBPAGE;
    settings->rank[6] = GROUPBY_FILESTRING;
    settings->rank[7] = GROUPBY_NAMESTRING;
    settings->rank[8] = GROUPBY_INAMESTRING;
    for (iItem=0; iItem<items; iItem++) {
        if ((code=match_string(item[iItem], groupbyName, GROUPING_NAMES, 0))<0)
            return bombre("invalid groupby syntax", groupby_usage, 0);
        else
            swapRanks(iItem, code, settings);
        }
    return 1;
    }

void swapRanks(long rank, long code, GROUPBY_SETTINGS *settings)
{
    long i, j;
    for (i=0; i<GROUPING_NAMES; i++) {
        if (settings->rank[i]==code) {
            if (rank!=i) {
                if (rank<i) {
                    for (j=i; j>rank && j>0; j--)
                        settings->rank[j] = settings->rank[j-1];
                    }
                else {
                    for (j=i; j<rank && j<(GROUPING_NAMES-1); j++)
                        settings->rank[j] = settings->rank[j+1];
                    }
                settings->rank[rank] = code;
                settings->ranksChanged = 1;
                }
            }
        }
    }

void determine_panel_assignments(PLOT_SPEC *plspec)
{
  long iset, ireq, panel, i, omnis, nLast, nFirst, newpanel, newpanelPending, startCount;
  PLOT_REQUEST *plreq;
  PLOT_DATA *dataset;

  for (ireq=1; ireq<plspec->plot_requests; ireq++)
    if (plspec->plot_request[ireq-1].flags&PLREQ_ENDPANEL)
      plspec->plot_request[ireq].flags |= PLREQ_NEWPANEL;
  
  perform_dataset_sort(plspec);

  plspec->panel = calloc(1, sizeof(*plspec->panel));
  plspec->panels = panel = 0;
  /* 
  plspec->panel[0].dataset = NULL;
  plspec->panel[0].datasets = 0;
  plspec->panel[0].flags = 0;
  */

  startCount = 0;
  newpanelPending = 0;
  for (iset=0; iset<plspec->datasets; iset++) {
    ireq = plspec->dataset[iset].request_index;
    if (plspec->plot_request[ireq].flags&PLREQ_OMNIPRESENT)
      continue;
    newpanel = 0;
    if (plspec->panel[panel].datasets) {
      if (plspec->plot_request[ireq].flags&PLREQ_NEWPANEL && ireq!=plspec->dataset[iset-1].virtual_request_index)
        newpanel = 1;
      else if (plspec->plot_request[ireq].flags&PLREQ_SEPARATE) {
        if (!(plspec->plot_request[ireq].separate_flags&~SEPARATE_GROUPSOF)) {
          /* The user gave -separate, but didn't give any options exclusive of the
           * groupsof parameter.  Group by page, keeping datapages together unless 
           * instructed otherwise.
           */
          if (newpanelPending || (iset-startCount)%plspec->plot_request[ireq].separate_group_size==0) {
            if (((plspec->plot_request[ireq].split.flags&SPLIT_PAGES 
                  && !(plspec->plot_request[ireq].split.flags&SPLIT_COMPLETELY)) 
                 || plspec->plot_request[ireq].flags&PLREQ_TRANSPOSE)
                && (plspec->dataset[iset].datapage==plspec->dataset[iset-1].datapage &&
                    plspec->dataset[iset].virtual_file_index==plspec->dataset[iset-1].virtual_file_index &&
                    plspec->dataset[iset].virtual_request_index==plspec->dataset[iset-1].virtual_request_index)) 
            {
              newpanelPending = 1;
              newpanel = 0;
              startCount++;
            }
            else {
              newpanel = 1;
              newpanelPending = 0;
              startCount = iset;
            }
          }
        }
        else {
          /* The user specified starting a new panel when the file index, request index,
           * name index, and/or page index changes, and possibly a number-to-group parameter.
           */   
          plreq = plspec->plot_request;
          dataset = plspec->dataset;
          if (
              (plreq[ireq].separate_flags&SEPARATE_FILEINDEX &&
               dataset[iset].virtual_file_index!=dataset[iset-1].virtual_file_index) 
              ||
              (plreq[ireq].separate_flags&SEPARATE_NAMEINDEX &&
               dataset[iset].virtual_dataname_index!=dataset[iset-1].virtual_dataname_index) 
              ||
              (plreq[ireq].separate_flags&SEPARATE_PAGE &&
               dataset[iset].datapage!=dataset[iset-1].datapage) 
              ||
              (plreq[ireq].separate_flags&SEPARATE_SUBPAGE &&
               dataset[iset].subpage!=dataset[iset-1].subpage) 
              ||
              (plreq[ireq].separate_flags&SEPARATE_REQUEST &&
               dataset[iset].virtual_request_index!=dataset[iset-1].virtual_request_index)
              ||
              (plreq[ireq].separate_flags&SEPARATE_TAG &&
               strcmp(dataset[iset].tag, dataset[iset-1].tag)!=0)
              ||
              (plreq[ireq].separate_flags&SEPARATE_FILESTRING &&
               strcmp(plreq[ireq].filename[dataset[iset].file_index],
                      plreq[dataset[iset-1].request_index].filename[dataset[iset-1].file_index])!=0)
              ||
              (plreq[ireq].separate_flags&SEPARATE_NAMESTRING &&
               datanameCompare(plreq+ireq, dataset[iset].dataname_index,
                               plreq+dataset[iset-1].request_index, dataset[iset-1].dataname_index, 0)!=0) ||
              (plreq[ireq].separate_flags&SEPARATE_INAMESTRING &&
               datanameCompare(plreq+ireq, dataset[iset].dataname_index,
                               plreq+dataset[iset-1].request_index, dataset[iset-1].dataname_index, 1)!=0))
          {
            if (!(plreq[ireq].separate_flags & SEPARATE_GROUPSOF)) {
              startCount = iset;
              newpanel = 1;
            }
            else {
              if ((iset-startCount)%plspec->plot_request[ireq].separate_group_size==0) {
                newpanel = 1;
                startCount = iset;
              }
            }
          }
        }
      }
    }

    if (newpanel) {
      plspec->panels = ++panel;
      plspec->panel = trealloc(plspec->panel, sizeof(*plspec->panel)*(panel+1));
      plspec->panel[panel].dataset = NULL;
      plspec->panel[panel].datasets = 0;
    }
#if defined(DEBUG)
    fprintf(stderr, "dataset %ld assigned to panel %ld, slot %ld\n",
            iset, panel, plspec->panel[panel].datasets);
#endif
    plspec->panel[panel].dataset = trealloc(plspec->panel[panel].dataset, 
                                            sizeof(PLOT_DATA)*(plspec->panel[panel].datasets+1));
    plspec->dataset[iset].plotpanel = panel;
    plspec->panel[panel].dataset[plspec->panel[panel].datasets] = &plspec->dataset[iset];
    plspec->dataset[iset].fromOmni = 0;
    plspec->panel[panel].datasets += 1;
  }
  plspec->panels ++;
#if defined(DEBUG)
  fprintf(stderr, "%ld panels created\n", plspec->panels);
  for (i=0; i<plspec->panels; i++)
    fprintf(stderr, "panel %ld:  datasets=%ld   dataset=%ux\n", i, plspec->panel[i].datasets,
            plspec->panel[i].dataset);
#endif
  omnis = nFirst = nLast = 0;
  for (iset=0; iset<plspec->datasets; iset++) {
    ireq = plspec->dataset[iset].request_index;
    if (plspec->plot_request[ireq].flags&PLREQ_OMNIPRESENT) {
      omnis++;
      if (plspec->plot_request[ireq].flags&PLREQ_OMNIFIRST)
	nFirst++;
      else
	nLast++;
      plspec->dataset[iset].plotpanel = -1;
    }
  }
#if defined(DEBUG)
  fprintf(stderr, "omnis=%ld, nFirst=%ld, nLast=%ld\n", omnis, nFirst, nLast);
#endif
  if (omnis) {
    long iFirst, iLast;
    for (panel=0; panel<plspec->panels; panel++) {
      plspec->panel[panel].dataset = 
        trealloc(plspec->panel[panel].dataset,
                 sizeof(*plspec->panel[panel].dataset)*(plspec->panel[panel].datasets+omnis));
      if (nFirst) {
	for (iset=plspec->panel[panel].datasets-1; iset>=0; iset--) {
#if defined(DEBUG)
	  fprintf(stderr, "Moving dataset from slot %ld to %ld\n",
		  iset, iset+nFirst);
#endif
	  plspec->panel[panel].dataset[iset+nFirst] = plspec->panel[panel].dataset[iset];
	}
      }
      for (iset=iFirst=iLast=0; iset<plspec->datasets; iset++) {
        ireq = plspec->dataset[iset].request_index;
        if (plspec->plot_request[ireq].flags&PLREQ_OMNIPRESENT) {
	  if (plspec->plot_request[ireq].flags&PLREQ_OMNIFIRST) {
	    i = iFirst;
	    iFirst ++;
	  } else {
	    i = iLast + plspec->panel[panel].datasets + nFirst;
	    iLast ++;
	  }
#if defined(DEBUG)
	  fprintf(stderr, "Placing omni dataset in slot %ld\n", i);
#endif
	  plspec->panel[panel].dataset[i] = tmalloc(sizeof(*plspec->panel[panel].dataset[i]));
	  memcpy(plspec->panel[panel].dataset[i],
		 plspec->dataset+iset, 
		 sizeof(*plspec->panel[panel].dataset[i]));
	  plspec->panel[panel].dataset[i]->plotpanel = panel;
	  plspec->panel[panel].dataset[i]->fromOmni = 1;
	  plspec->panel[panel].dataset[i]->omniSourceDataset = iset;
        }
      }
      plspec->panel[panel].datasets += omnis;
    }
#if defined(DEBUG)
    for (panel=0; panel<plspec->panels; panel++)
      for (iset=0; iset<plspec->panel[panel].datasets; iset++) 
        fprintf(stderr, "data set %ld of panel %ld at address %x\n",
                iset, panel, plspec->panel[panel].dataset[iset]);
#endif
  }
  determine_panel_equivalent_requests(plspec);
}

static PLOT_SPEC *plspecSort;
static long *groupSortOrder = NULL;

int groupBy_cmpasc(const void *a, const void *b)
{
    const PLOT_DATA *setA, *setB;
    long delta, i;
    double diff;
    double tagA, tagB;
    long scanA, scanB;
    
    setA = (PLOT_DATA *)a;
    setB = (PLOT_DATA *)b;
    scanA=scanB=0;
    tagA=tagB=0;
    
#if defined(DEBUG)
    fprintf(stderr, "%x vs %x: ", setA, setB); 
    fprintf(stderr, "(%ld, %ld, %ld, %ld, %ld) vs (%ld, %ld, %ld, %ld, %ld)\n",
           setA->virtual_request_index, setA->virtual_file_index, setA->virtual_dataname_index, setA->datapage, setA->subpage,
           setB->virtual_request_index, setB->virtual_file_index, setB->virtual_dataname_index, setB->datapage, setB->subpage);
#endif

    for (i=0; i<GROUPING_NAMES; i++) {
#if defined(DEBUG)
        fprintf(stderr, "  checking %s (%ld) ", groupbyName[groupSortOrder[i]], groupSortOrder[i]);
#endif
        switch (groupSortOrder[i]) {
          case GROUPBY_FILEINDEX:
            delta = setA->virtual_file_index - setB->virtual_file_index;
            break;
          case GROUPBY_FILESTRING:
            delta = strcmp(plspecSort->plot_request[setA->request_index].filename[setA->file_index],
                           plspecSort->plot_request[setB->request_index].filename[setB->file_index]);
            break;
          case GROUPBY_PAGE:
            delta = setA->datapage - setB->datapage;
            break;
          case GROUPBY_SUBPAGE:
            delta = setA->subpage - setB->subpage;
            break;
          case GROUPBY_NAMEINDEX:
            delta = setA->virtual_dataname_index - setB->virtual_dataname_index;
            break;
          case GROUPBY_NAMESTRING:
            delta = datanameCompare(plspecSort->plot_request+setA->request_index, setA->dataname_index,
                                    plspecSort->plot_request+setB->request_index, setB->dataname_index, 0);
            break;
          case GROUPBY_INAMESTRING:
            delta = datanameCompare(plspecSort->plot_request+setA->request_index, setA->dataname_index,
                                       plspecSort->plot_request+setB->request_index, setB->dataname_index, 1);
            break;
          case GROUPBY_REQUEST:
            delta = setA->virtual_request_index - setB->virtual_request_index;
            break;
          case GROUPBY_TAG:
            delta = 0;
            if (!setA->tag && setB->tag)
              delta=1;
            if (setA->tag && !setB->tag)
              delta=-1;
            if (setA->tag && setB->tag) {
              scanA=sscanf(setA->tag,"%lf",&tagA);
              scanB=sscanf(setB->tag,"%lf",&tagB);
              if (scanA==1 && scanB!=1)
                delta=1;
              if (scanA!=1 && scanB==1)
                delta=-1;
              if (scanA==1 && scanB==1) {
                /*both are numbers */
                if (tagA<tagB)
                  delta=-1;
                if (tagA>tagB)
                  delta=1;
              }
              if (scanA !=1 && scanB!=1) {
                /*both are strings */
                if ((diff = strcmp(setA->tag , setB->tag))>0)
                  delta = 1;
                else if (diff<0)
                  delta = -1;
              }
            } 
            break;
          default:
            bomb("invalid sort item (groupBy_cmpasc)", NULL);
	    exit(1);
            }
        if (delta) {
#if defined(DEBUG)
            fprintf(stderr, "data sets distinguished by %s (%ld)\n", groupbyName[groupSortOrder[i]],
                    groupSortOrder[i]);
#endif
            return delta;
            }
        }
#if defined(DEBUG)
    fprintf(stderr, "subpage comparison occurred\n");
#endif
    return setA->subpage - setB->subpage;
    }

void perform_dataset_sort(PLOT_SPEC *plspec)
{
    long ireq1, ireq2;
    long iset1, iset2;


    if (!plspec->groupBy.ranksChanged)
        return; 

    plspecSort = plspec;
    groupSortOrder = plspec->groupBy.rank;

#if defined(DEBUG)
    for (iset1=0; iset1<plspec->datasets; iset1++)
        fprintf(stderr, "set %ld: req=%ld, file=%ld, data=%ld, page=%ld, subpage=%ld  ptr=%x\n",
               iset1,
               plspec->dataset[iset1].virtual_request_index,
               plspec->dataset[iset1].virtual_file_index,
               plspec->dataset[iset1].virtual_dataname_index,
               plspec->dataset[iset1].datapage, plspec->dataset[iset1].subpage,
               plspec->dataset+iset1);
#endif

    iset2 = 0;
    for (ireq1=1; ireq1<plspec->plot_requests; ) {
        for (ireq2=ireq1+1; ireq2<plspec->plot_requests; ireq2++) {
            if (plspec->plot_request[ireq2].flags&PLREQ_NEWPANEL)
                break;
            }
        iset1 = iset2;
        for (; iset2<plspec->datasets; iset2++) {
            if (plspec->dataset[iset2].virtual_request_index<ireq1)
                bomb("dataset ordering problem (perform_dataset_sort)", NULL);
            if (plspec->dataset[iset2].virtual_request_index>=ireq2)
                break;
            }
        if (iset2!=iset1)
            qsort((void*)(plspec->dataset+iset1), iset2-iset1, sizeof(*plspec->dataset), groupBy_cmpasc);
        ireq1 = ireq2;
        }

#if defined(DEBUG)
    for (iset1=0; iset1<plspec->datasets; iset1++)
        fprintf(stderr, "set %ld: req=%ld, file=%ld, data=%ld, page=%ld, subpage=%ld  ptr=%x\n",
               iset1,
               plspec->dataset[iset1].virtual_request_index,
               plspec->dataset[iset1].virtual_file_index,
               plspec->dataset[iset1].virtual_dataname_index,
               plspec->dataset[iset1].datapage, plspec->dataset[iset1].subpage,
               plspec->dataset+iset1);
#endif
    }

int strcmpWithCheck(char *ptr1, char *ptr2, long invertStrings)
{
  char *ptr1i, *ptr2i;
  int diff;
  
  if (!ptr1 && !ptr2)
    return 0;
  if (ptr1 && !ptr2)
    return 1;
  if (!ptr1 && ptr2)
    return -1;
  if (!invertStrings) 
    return strcmp_nh(ptr1, ptr2);

  /* compare strings starting at the end */
  ptr1i = ptr1+strlen(ptr1)-1;
  ptr2i = ptr2+strlen(ptr2)-1;
  do {
    if ((diff = *ptr1i - *ptr2i)) {
      return diff;
    }
    ptr1i --;
    ptr2i --;
    if (ptr1i<ptr1) {
      if (ptr2i<ptr2) 
        return 0;
      return -1;
    }
    if (ptr2i<ptr2)
      return 1;
  } while (1);
}


long datanameCompare(PLOT_REQUEST *plreq1, long index1, PLOT_REQUEST *plreq2, long index2, long invertStrings)
{
    long delta;
    if (plreq1->xname || plreq2->xname) {
        if (plreq1->xname && !plreq2->xname)
            return 1;
        if (!plreq1->xname && plreq2->xname)
            return -1;
        if ((delta=strcmpWithCheck(plreq1->xname[index1], plreq2->xname[index2], invertStrings))!=0 )
            return delta;
        }
    if (plreq1->yname || plreq2->yname) {
        if (plreq1->yname && !plreq2->yname)
            return 1;
        if (!plreq1->yname && plreq2->yname)
            return -1;
        if ((delta=strcmpWithCheck(plreq1->yname[index1], plreq2->yname[index2], invertStrings))!=0 )
            return delta;
        }
    if (plreq1->y1name[index1] && !plreq2->y1name[index2])
        return 1;
    if (!plreq1->y1name[index1] && plreq2->y1name[index2])
        return -1;
    if (plreq1->x1name[index1] && !plreq2->x1name[index2])
        return 1;
    if (!plreq1->x1name[index1] && plreq2->x1name[index2])
        return -1;
    if (plreq1->y1name[index1] && 
        (delta=strcmpWithCheck(plreq1->y1name[index1], plreq2->y1name[index2], invertStrings)!=0))
        return delta;
    if (plreq1->x1name[index1] && 
        (delta=strcmpWithCheck(plreq1->x1name[index1], plreq2->x1name[index2], invertStrings)!=0))
        return delta;
    return 0;
    }


void determine_panel_equivalent_requests(PLOT_SPEC *plspec)
/* Look at all the plot requests that are involved in making each
 * panel, and compose a single, equivalent plot request for
 * each panel.  Makes it easy to figure out how to make each
 * panel later.
 */
{
  long ipanel, iset, ireq, j;
  long *requestIndex;
  PLOT_PANEL *panel;
  PLOT_REQUEST *request;
  
  if (!(requestIndex = SDDS_Malloc(sizeof(*requestIndex)*plspec->datasets)))
    SDDS_Bomb("Memory allocation failure (determine_panel_equivalent_requests)");
  request = plspec->plot_request;
  for (ipanel=0; ipanel<plspec->panels; ipanel++) {
    /* make a list of the requests used by datasets on this panel */
    panel = plspec->panel+ipanel;
    for (iset=0; iset<panel->datasets; iset++)
      requestIndex[iset] = panel->dataset[iset]->request_index;
    qsort((void*)requestIndex, panel->datasets, sizeof(*requestIndex), long_cmpasc);

    panel->flags = panel->mode = panel->joinScaleFlags = 0;
    panel->linetypeDefault = panel->linethicknessDefault = panel->axesLinetype = panel->axesLinethickness = 0;
    panel->titleSpec[0].flags = panel->titleSpec[1].flags = 0;
    panel->titleSpec[0].label = panel->titleSpec[1].label = NULL;
    panel->titleSpec[0].edit_command = panel->titleSpec[1].edit_command = NULL;
    panel->legend.code = 0;
    panel->legend.value = panel->legend.edit_command = NULL;
    panel->legend.scale = 1;
    panel->legend.thickness = 0;
    panel->labelSize = 0.03;
    
    for (iset=0; iset<panel->datasets; iset++) {
      if (iset && requestIndex[iset]==requestIndex[iset-1])
        continue;
      ireq = requestIndex[iset];
      panel->flags |= request[ireq].flags;
      panel->joinScaleFlags |= request[ireq].joinScaleFlags;
      panel->mode  |= request[ireq].mode;
      if (request[ireq].labelsize_fraction>0)
        panel->labelSize = request[ireq].labelsize_fraction;
      if (request[ireq].flags&PLREQ_LSPACE_GIVEN)
        memcpy(panel->lspace, request[ireq].lspace, sizeof(*request[ireq].lspace)*4);
      if (request[ireq].legend.code)
        panel->legend = request[ireq].legend;
      if (request[ireq].linetype_default)
        panel->linetypeDefault = request[requestIndex[iset]].linetype_default;
      if (request[ireq].linethickness_default)
        panel->linethicknessDefault = request[requestIndex[iset]].linethickness_default;
      if (request[ireq].axesLinetype &&
          (request[ireq].flags&PLREQ_AXESX || request[ireq].flags&PLREQ_AXESY))
        panel->axesLinetype = request[ireq].axesLinetype;
      if (request[ireq].axesLinethickness &&
          (request[ireq].flags&PLREQ_AXESX || request[ireq].flags&PLREQ_AXESY))
        panel->axesLinethickness = request[ireq].axesLinethickness;
      /* title and topline */
      for (j=0; j<2; j++) {
        if (request[ireq].label[j+2].flags&(LABEL_PARAMETER_GIVEN+LABEL_EDITCOMMAND_GIVEN+
                                            LABEL_USE_DEFAULT+LABEL_STRING_GIVEN+
                                            LABEL_USE_NAME+LABEL_USE_SYMBOL+LABEL_USE_DESCRIPTION+
                                            LABEL_INCLUDE_UNITS))
          panel->titleSpec[j] = request[ireq].label[j+2];
        if (panel->dataset[iset]->label[j+2])
          panel->titleSpec[j].label = panel->dataset[iset]->label[j+2];
      }
    }
  }
  free(requestIndex);
}

void resolve_omnipresent_references(PLOT_SPEC *plspec)
{
  long panel, panelSet, origDataset, i, plane;
  PLOT_DATA saveDataset;
  for (panel=0; panel<plspec->panels; panel++) {
    for (panelSet=0; panelSet<plspec->panel[panel].datasets; panelSet++) {
      if (!plspec->panel[panel].dataset[panelSet]->fromOmni)
        continue;
      origDataset = plspec->panel[panel].dataset[panelSet]->omniSourceDataset;
      memcpy(&saveDataset, plspec->panel[panel].dataset[panelSet],
             sizeof(saveDataset));
      memcpy(plspec->panel[panel].dataset[panelSet],
             &plspec->dataset[origDataset], sizeof(plspec->dataset[origDataset]));
      for (plane=0; plane<2; plane++) {
        plspec->panel[panel].dataset[panelSet]->scalesGroupIndex[plane] = 
          saveDataset.scalesGroupIndex[plane];
        plspec->panel[panel].dataset[panelSet]->scalesGroupID[plane] = 
          saveDataset.scalesGroupID[plane];
      }
      plspec->panel[panel].dataset[panelSet]->plotpanel = 
        saveDataset.plotpanel;
      for (i=0; i<4; i++)
        plspec->panel[panel].dataset[panelSet]->limit[i] =
          saveDataset.limit[i];
    }
  }
}

