/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2016 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsv�gen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "stdafx.h"

#include "resource.h"

#include <commctrl.h>
#include <commdlg.h>

#include "oEvent.h"
#include "xmlparser.h"
#include "gdioutput.h"
#include "gdifonts.h"

#include "csvparser.h"
#include "SportIdent.h"
#include "meos_util.h"
#include "oListInfo.h"

#include "TabSpeaker.h"
#include "TabList.h"

#include <cassert>

//Base position for speaker buttons
#define SPEAKER_BASE_X 40

TabSpeaker::TabSpeaker(oEvent *poe):TabBase(poe)
{
  classId=0;
  ownWindow = false;

  lastControlToWatch = 0;
  lastClassToWatch = 0;
  watchLevel = oTimeLine::PMedium;
  watchNumber = 5;
}

TabSpeaker::~TabSpeaker()
{
}


int tabSpeakerCB(gdioutput *gdi, int type, void *data)
{
  TabSpeaker &ts = dynamic_cast<TabSpeaker &>(*gdi->getTabs().get(TSpeakerTab));

  switch(type){
    case GUI_BUTTON: {
      //Make a copy
      ButtonInfo bu=*static_cast<ButtonInfo *>(data);
      return ts.processButton(*gdi, bu);
    }
    case GUI_LISTBOX:{
      ListBoxInfo lbi=*static_cast<ListBoxInfo *>(data);
      return ts.processListBox(*gdi, lbi);
    }
    case GUI_EVENT: {
      EventInfo ei=*static_cast<EventInfo *>(data);
      return ts.handleEvent(*gdi, ei);
    }
    case GUI_CLEAR:
      return ts.onClear(*gdi);
    case GUI_TIMEOUT:
    case GUI_TIMER:
      ts.updateTimeLine(*gdi);
    break;
  }
  return 0;
}

int TabSpeaker::handleEvent(gdioutput &gdi, const EventInfo &ei)
{
  updateTimeLine(gdi);
  return 0;
}

int TabSpeaker::processButton(gdioutput &gdi, const ButtonInfo &bu)
{
  if (bu.id=="Settings") {
    if (controlsToWatch.empty()) {
      // Get default
      vector<pControl> ctrl;
      oe->getControls(ctrl, true);
      for (size_t k = 0; k < ctrl.size(); k++) {
        if (ctrl[k]->isValidRadio()) {
          vector<int> cc;
          ctrl[k]->getCourseControls(cc);
          controlsToWatch.insert(cc.begin(), cc.end());
        }
      }
    }
    gdi.restore("settings");
    gdi.unregisterEvent("DataUpdate");
    gdi.fillDown();
    gdi.addString("", boldLarge, "Speakerst�d");
    gdi.addString("", 0, "help:speaker_setup");
    gdi.dropLine(1);
    gdi.addCheckbox("ShortNames", "Use initials in names", 0, oe->getPropertyInt("SpeakerShortNames", false) != 0);
    gdi.dropLine(0.5);

    gdi.pushX();
    gdi.fillRight();
    gdi.addListBox("Classes", 200, 300, 0,"Klasser","", true);

    oe->fillClasses(gdi, "Classes", oEvent::extraNone, oEvent::filterNone);
    gdi.setSelection("Classes", classesToWatch);

    gdi.addListBox("Controls", 200, 300, 0, "Kontroller","", true);
    gdi.pushX();
    gdi.fillDown();

    vector< pair<string, size_t> > d;
    oe->fillControls(d, oEvent::CTCourseControl);
    gdi.addItem("Controls", d);

    gdi.setSelection("Controls", controlsToWatch);

    gdi.dropLine();
    gdi.addButton("OK", "OK", tabSpeakerCB).setDefault();
    gdi.addButton("Cancel", "Avbryt", tabSpeakerCB).setCancel();

    gdi.refresh();
  }
  else if (bu.id=="ZoomIn") {
    gdi.scaleSize(1.05);
  }
  else if (bu.id=="ZoomOut") {
    gdi.scaleSize(1.0/1.05);
  }
  else if (bu.id=="Manual") {
    gdi.unregisterEvent("DataUpdate");
    gdi.restore("settings");
    gdi.fillDown();
    gdi.addString("", boldLarge, "Inmatning av mellantider");
    gdi.dropLine(0.5);
    manualTimePage(gdi);
  }
  else if (bu.id == "PunchTable") {
    gdi.clearPage(false);
    gdi.addButton("Cancel", "St�ng", tabSpeakerCB);
    gdi.dropLine();
    gdi.addTable(oe->getPunchesTB(), gdi.getCX(), gdi.getCY());
    gdi.refresh();
  }
  else if (bu.id == "Priority") {
    gdi.clearPage(false);
    gdi.addString("", boldLarge, "Bevakningsprioritering");
    gdi.addString("", 10, "help:speakerprio");
    gdi.dropLine();
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", 0, "Klass:");
    gdi.addSelection("Class", 200, 200, tabSpeakerCB, "", "V�lj klass");
    oe->fillClasses(gdi, "Class", oEvent::extraNone, oEvent::filterNone);
    gdi.addButton("ClosePri", "St�ng", tabSpeakerCB);
    gdi.dropLine(2);
    gdi.popX();
    gdi.refresh();
  }
  else if (bu.id == "ClosePri") {
    savePriorityClass(gdi);
    loadPage(gdi);
  }
  else if (bu.id == "LiveResult") {
    gdioutput *gdi_new = createExtraWindow(uniqueTag("list"), MakeDash("MeOS - Live"), gdi.getWidth() + 64 + gdi.scaleLength(120));
       
    gdi_new->clearPage(false);
    gdi_new->addString("", boldLarge, "Liveresultat");
    gdi_new->addString("", 10, "help:liveresultat");
    gdi_new->dropLine();
    gdi_new->pushY();
    gdi_new->pushX();

    TabList::makeClassSelection(*gdi_new);
    oe->fillClasses(*gdi_new, "ListSelection", oEvent::extraNone, oEvent::filterNone);
    
    gdi_new->popY();
    gdi_new->setCX(gdi_new->getCX() + gdi_new->scaleLength(280));
    gdi_new->fillRight();
    gdi_new->pushX();
    TabList::makeFromTo(*gdi_new);
    TabList::enableFromTo(*oe, *gdi_new, true, true);

    gdi_new->fillRight();
    gdi.dropLine();
    gdi_new->addButton("StartLive", "Starta", tabSpeakerCB).setDefault();
    gdi_new->addButton("CancelClose", "Avbryt", tabSpeakerCB).setCancel();

    gdi_new->refresh();
  }
  else if (bu.id == "StartLive") {
    TabList &ts = dynamic_cast<TabList &>(*gdi.getTabs().get(TListTab));
        
    oListInfo li;
    oListParam &par = li.getParam();;
    par.useControlIdResultTo = gdi.getSelectedItem("ResultSpecialTo").first;
    par.useControlIdResultFrom = gdi.getSelectedItem("ResultSpecialFrom").first;
    gdi.getSelection("ListSelection", par.selection);
    
    gdi.clearPage(false);
    gdi.setFullScreen(true);
    gdi.hideBackground(true);
    ts.liveResult(gdi, li);
  }
  else if (bu.id == "Events") {
    gdi.restore("classes");

    shownEvents.clear();
    events.clear();

    drawTimeLine(gdi);
  }
  else if (bu.id == "Window") {
    oe->setupTimeLineEvents(0);

    gdioutput *gdi_new = createExtraWindow(uniqueTag("speaker"), MakeDash("MeOS - Speakerst�d"), gdi.getWidth() + 64 + gdi.scaleLength(120));
    if (gdi_new) {
      TabSpeaker &tl = dynamic_cast<TabSpeaker &>(*gdi_new->getTabs().get(TSpeakerTab));
      tl.ownWindow = true;
      tl.loadPage(*gdi_new);
      //oe->renderTimeLineEvents(*gdi_new);
    }
  }
  else if (bu.id=="StoreTime") {
    storeManualTime(gdi);
  }
  else if (bu.id=="Cancel") {
    loadPage(gdi);
  }
  else if (bu.id=="CancelClose") {
    gdi.closeWindow();
  }
  else if (bu.id=="OK") {
    gdi.getSelection("Classes", classesToWatch);
    gdi.getSelection("Controls", controlsToWatch);
    if (controlsToWatch.empty())
      controlsToWatch.insert(-2); // Non empty but no control

    controlsToWatchSI.clear();
    for (set<int>::iterator it=controlsToWatch.begin();it!=controlsToWatch.end();++it) {
      pControl pc=oe->getControl(*it, false);
      if (pc) {
        pc->setRadio(true);
        pc->synchronize(true);
        controlsToWatchSI.insert(pc->Numbers, pc->Numbers+pc->nNumbers);
      }
    }
    oe->setProperty("SpeakerShortNames", (int)gdi.isChecked("ShortNames"));
    loadPage(gdi);
  }
  else if (bu.id.substr(0, 3)=="cid" ) {
    classId=atoi(bu.id.substr(3, string::npos).c_str());
    bool shortNames = oe->getPropertyInt("SpeakerShortNames", false) != 0;
    generateControlList(gdi, classId);
    gdi.setRestorePoint("speaker");
    gdi.setRestorePoint("SpeakerList");

    if (selectedControl.count(classId)==1)
      oe->speakerList(gdi, classId, selectedControl[classId].getLeg(),
                                    selectedControl[classId].getControl(),
                                    selectedControl[classId].getPreviousControl(),
                                    selectedControl[classId].isTotal(),
                                    shortNames);
  }
  else if (bu.id.substr(0, 4)=="ctrl") {
    bool shortNames = oe->getPropertyInt("SpeakerShortNames", false) != 0;
    int ctrl = atoi(bu.id.substr(4, string::npos).c_str());
    int ctrlPrev = bu.getExtraInt();
    selectedControl[classId].setControl(ctrl, ctrlPrev);
    gdi.restore("speaker");
    oe->speakerList(gdi, classId, selectedControl[classId].getLeg(), ctrl, ctrlPrev,
                                  selectedControl[classId].isTotal(), shortNames);
  }

  return 0;
}

void TabSpeaker::drawTimeLine(gdioutput &gdi) {
  gdi.restoreNoUpdate("SpeakerList");
  gdi.setRestorePoint("SpeakerList");

  gdi.fillRight();
  gdi.pushX();
  gdi.dropLine(0.3);
  gdi.addString("", 0, "Filtrering:");
  gdi.dropLine(-0.2);
  gdi.addSelection("DetailLevel", 160, 100, tabSpeakerCB);
  gdi.addItem("DetailLevel", lang.tl("Alla h�ndelser"), oTimeLine::PLow);
  gdi.addItem("DetailLevel", lang.tl("Viktiga h�ndelser"), oTimeLine::PMedium);
  gdi.addItem("DetailLevel", lang.tl("Avg�rande h�ndelser"), oTimeLine::PHigh);
  gdi.selectItemByData("DetailLevel", watchLevel);

  gdi.dropLine(0.2);
  gdi.addString("", 0, "Antal:");
  gdi.dropLine(-0.2);
  gdi.addSelection("WatchNumber", 160, 200, tabSpeakerCB);
  gdi.addItem("WatchNumber", lang.tl("X senaste#5"), 5);
  gdi.addItem("WatchNumber", lang.tl("X senaste#10"), 10);
  gdi.addItem("WatchNumber", lang.tl("X senaste#20"), 20);
  gdi.addItem("WatchNumber", lang.tl("X senaste#50"), 50);
  gdi.addItem("WatchNumber", "Alla", 0);
  gdi.selectItemByData("WatchNumber", watchNumber);
  gdi.dropLine(2);
  gdi.popX();

  string cls;
  for (set<int>::iterator it = classesToWatch.begin(); it != classesToWatch.end(); ++it) {
    pClass pc = oe->getClass(*it);
    if (pc) {
      if (!cls.empty())
        cls += ", ";
      cls += oe->getClass(*it)->getName();
    }
  }
  gdi.fillDown();
  gdi.addString("", 1, "Bevakar h�ndelser i X#" + cls);
  gdi.dropLine();

  gdi.setRestorePoint("TimeLine");
  updateTimeLine(gdi);
}

void TabSpeaker::updateTimeLine(gdioutput &gdi) {
  int storedY = gdi.GetOffsetY();
  int storedHeight = gdi.getHeight();
  bool refresh = gdi.hasData("TimeLineLoaded");

  if (refresh)
    gdi.takeShownStringsSnapshot();

  gdi.restoreNoUpdate("TimeLine");
  gdi.setRestorePoint("TimeLine");
  gdi.setData("TimeLineLoaded", 1);

  gdi.registerEvent("DataUpdate", tabSpeakerCB);
  gdi.setData("DataSync", 1);
  gdi.setData("PunchSync", 1);

  gdi.pushX(); gdi.pushY();
  gdi.updatePos(0,0,0, storedHeight);
  gdi.popX(); gdi.popY();
  gdi.SetOffsetY(storedY);

  int nextEvent = oe->getTimeLineEvents(classesToWatch, events, shownEvents, 0);

  oe->updateComputerTime();
  int timeOut = nextEvent * 1000 - oe->getComputerTimeMS();

  string str = "Now: " + oe->getAbsTime(oe->getComputerTime()) + " Next:" + oe->getAbsTime(nextEvent) + " Timeout:" + itos(timeOut) + "\n";
  OutputDebugString(str.c_str());

  if (timeOut > 0) {
    gdi.addTimeoutMilli(timeOut, "timeline", tabSpeakerCB);
    //gdi.addTimeout(timeOut, tabSpeakerCB);
  }
  bool showClass = classesToWatch.size()>1;

  oListInfo li;
  const int classWidth = gdi.scaleLength(li.getMaxCharWidth(oe, classesToWatch, lClassName, "", normalText, false));
  const int timeWidth = gdi.scaleLength(60);
  const int totWidth = gdi.scaleLength(450);
  const int extraWidth = gdi.scaleLength(200);
  string dash = MakeDash("- ");
  const int extra = 5;

  int limit = watchNumber > 0 ? watchNumber : events.size();
  for (size_t k = events.size()-1; signed(k) >= 0; k--) {
    oTimeLine &e = events[k];

    if (e.getPriority() < watchLevel)
      continue;

    if (--limit < 0)
      break;

    int t = e.getTime();
    string msg = t>0 ? oe->getAbsTime(t) : MakeDash("--:--:--");
    pRunner r = pRunner(e.getSource(*oe));

    RECT rc;
    rc.top = gdi.getCY();
    rc.left = gdi.getCX();

    int xp = rc.left + extra;
    int yp = rc.top + extra;

    bool largeFormat = e.getPriority() >= oTimeLine::PHigh && k + 15 >= events.size();

    if (largeFormat) {
      gdi.addStringUT(yp, xp, 0, msg);

      int dx = 0;
      if (showClass) {
        pClass pc = oe->getClass(e.getClassId());
        if (pc) {
          gdi.addStringUT(yp, xp + timeWidth, fontMediumPlus, pc->getName());
          dx = classWidth;
        }
      }

      if (r) {
        string bib = r->getBib();
        if (!bib.empty())
          msg = bib + ", ";
        else
          msg = "";
        msg += r->getCompleteIdentification();
        int xlimit = totWidth + extraWidth - (timeWidth + dx);
        gdi.addStringUT(yp, xp + timeWidth + dx, fontMediumPlus, msg, xlimit);
      }

      yp += int(gdi.getLineHeight() * 1.5);
      msg = dash + lang.tl(e.getMessage());
      gdi.addStringUT(yp, xp + 10, breakLines, msg, totWidth);

      const string &detail = e.getDetail();

      if (!detail.empty()) {
        gdi.addString("", gdi.getCY(), xp + 20, 0, detail).setColor(colorDarkGrey);
      }

      if (r && e.getType() == oTimeLine::TLTFinish && r->getStatus() == StatusOK) {
        splitAnalysis(gdi, xp + 20, gdi.getCY(), r);
      }

      GDICOLOR color = colorLightRed;

      switch (e.getType()) {
        case oTimeLine::TLTFinish:
          if (r && r->statusOK())
            color = colorLightGreen;
          else
            color = colorLightRed;
          break;
        case oTimeLine::TLTStart:
          color = colorLightYellow;
          break;
        case oTimeLine::TLTRadio:
          color = colorLightCyan;
          break;
        case oTimeLine::TLTExpected:
          color = colorLightBlue;
          break;
      }

      rc.bottom = gdi.getCY() + extra;
      rc.right = rc.left + totWidth + extraWidth + 2 * extra;
      gdi.addRectangle(rc, color, true);
      gdi.dropLine(0.5);
    }
    else {
      if (showClass) {
        pClass pc = oe->getClass(e.getClassId());
        if (pc )
          msg += " (" + pc->getName() + ") ";
        else
          msg += " ";
      }
      else
        msg += " ";

      if (r) {
        msg += r->getCompleteIdentification() + " ";
      }
      msg += lang.tl(e.getMessage());
      gdi.addStringUT(gdi.getCY(), gdi.getCX(), breakLines, msg, totWidth);

      const string &detail = e.getDetail();

      if (!detail.empty()) {
        gdi.addString("", gdi.getCY(), gdi.getCX()+50, 0, detail).setColor(colorDarkGrey);
      }

      if (r && e.getType() == oTimeLine::TLTFinish && r->getStatus() == StatusOK) {
        splitAnalysis(gdi, xp, gdi.getCY(), r);
      }
      gdi.dropLine(0.5);
    }
  }

  if (refresh)
    gdi.refreshSmartFromSnapshot(false);
  else {
    if (storedHeight == gdi.getHeight())
      gdi.refreshFast();
    else
      gdi.refresh();
  }
}

void TabSpeaker::splitAnalysis(gdioutput &gdi, int xp, int yp, pRunner r)
{
  if (!r)
    return;

  vector<int> delta;
  r->getSplitAnalysis(delta);
  string timeloss = lang.tl("Bommade kontroller: ");
  pCourse pc = 0;
  bool first = true;
  const int charlimit = 90;
  for (size_t j = 0; j<delta.size(); ++j) {
    if (delta[j] > 0) {
      if (pc == 0) {
        pc = r->getCourse(true);
        if (pc == 0)
          break;
      }

      if (!first)
        timeloss += " | ";
      else
        first = false;

      timeloss += pc->getControlOrdinal(j) + ". " + formatTime(delta[j]);
    }
    if (timeloss.length() > charlimit || (!timeloss.empty() && !first && j+1 == delta.size())) {
      gdi.addStringUT(yp, xp, 0, timeloss).setColor(colorDarkRed);
      yp += gdi.getLineHeight();
      timeloss = "";
    }
  }
  if (first) {
    gdi.addString("", yp, xp, 0, "Inga bommar registrerade").setColor(colorDarkGreen);
  }
}

pCourse deduceSampleCourse(pClass pc, vector<oClass::TrueLegInfo > &stages, int leg);

void TabSpeaker::generateControlList(gdioutput &gdi, int classId)
{
  pClass pc=oe->getClass(classId);

  if (!pc)
    return;

  bool keepLegs = false;
  if (gdi.hasField("Leg")) {
    DWORD clsSel = 0;
    if (gdi.getData("ClassSelection", clsSel) && clsSel == pc->getId()) {
      gdi.restore("LegSelection", true);
      keepLegs = true;
    }
  }

  if (!keepLegs)
    gdi.restore("classes", true);

  gdi.setData("ClassSelection", pc->getId());

  pCourse course=0;

  int h,w;
  gdi.getTargetDimension(w, h);

  gdi.fillDown();

  int bw=gdi.scaleLength(100);
  int nbtn=max((w-80)/bw, 1);
  bw=(w-80)/nbtn;
  int basex=SPEAKER_BASE_X;
  int basey=gdi.getCY()+4;

  int cx=basex;
  int cy=basey;
  int cb=1;

  vector<oClass::TrueLegInfo> stages;
  pc->getTrueStages(stages);
  int leg = selectedControl[pc->getId()].getLeg();
  const bool multiDay = oe->hasPrevStage();

  if (stages.size()>1 || multiDay) {
    if (!keepLegs) {
      gdi.setData("CurrentY", cy);
      gdi.addSelection(cx, cy+2, "Leg", int(bw/gdi.getScale())-5, 100, tabSpeakerCB);

      if (stages.size() > 1) {
        if (leg == 0 && stages[0].first != 0) {
           leg = stages[0].first;
           selectedControl[pc->getId()].setLeg(selectedControl[pc->getId()].isTotal(), leg);
        }
        for (size_t k=0; k<stages.size(); k++) {
          gdi.addItem("Leg", lang.tl("Str�cka X#" + itos(stages[k].second)), stages[k].first);
        }
        if (multiDay) {
          for (size_t k=0; k<stages.size(); k++) {
            gdi.addItem("Leg", lang.tl("Str�cka X (total)#" + itos(stages[k].second)), 1000 + stages[k].first);
          }
        }
      }
      else if (stages.size() == 1) {
         if (leg == 0 && stages[0].first != 0)
           leg = stages[0].first;
         gdi.addItem("Leg", lang.tl("Etappresultat"), stages[0].first);
         gdi.addItem("Leg", lang.tl("Totalresultat"), 1000 + stages[0].first);
      }

      gdi.selectItemByData("Leg", leg);
      gdi.setRestorePoint("LegSelection");
    }
    else {
      gdi.getData("CurrentY", *(DWORD*)&cy);
    }
    cb+=1;
    cx+=1*bw;
  }
  else if (stages.size() == 1) {
    selectedControl[classId].setLeg(false, stages[0].first);
    leg = stages[0].first;
  }

  course = deduceSampleCourse(pc, stages, leg);
  /*
  int courseLeg = leg;
  for (size_t k = 0; k <stages.size(); k++) {
    if (stages[k].first == leg) {
      courseLeg = stages[k].nonOptional;
      break;
    }
  }

  if (pc->hasMultiCourse())
    course = pc->getCourse(courseLeg, 0, true);
  else
    course = pc->getCourse(true);
  */
  vector<pControl> controls;

  if (course)
    course->getControls(controls);
  int previousControl = 0;
  for (size_t k = 0; k < controls.size(); k++) {
    int cid = course->getCourseControlId(k);
    if (controlsToWatch.count(cid) ) {

      if (selectedControl[classId].getControl() == -1) {
        // Default control
        selectedControl[classId].setControl(cid, previousControl);
      }

      char bf[16];
      sprintf_s(bf, "ctrl%d", cid);
      string name = course->getRadioName(cid);
      /*if (controls[k]->hasName()) {
        name = "#" + controls[k]->getName();
        if (controls[k]->getNumberDuplicates() > 1)
          name += "-" + itos(numbering++);
      }
      else {
        name = lang.tl("radio X#" + itos(numbering++));
        capitalize(name);
        name = "#" + name;
      }
      */
      string tooltip = lang.tl("kontroll X (Y)#" + itos(k+1) +"#" + itos(controls[k]->getFirstNumber()));
      capitalize(tooltip);
      ButtonInfo &bi = gdi.addButton(cx, cy, bw, bf, "#" + name, tabSpeakerCB, "#" + tooltip, false, false);
      bi.setExtra(previousControl);
      previousControl = cid;
      cx+=bw;

      cb++;
      if (cb>nbtn) {
        cb=1;
        cy+=gdi.getButtonHeight()+4;
        cx=basex;
      }
    }
  }
  gdi.fillDown();
  char bf[16];
  sprintf_s(bf, "ctrl%d", oPunch::PunchFinish);
  gdi.addButton(cx, cy, bw, bf, "M�l", tabSpeakerCB, "", false, false).setExtra(previousControl);

  if (selectedControl[classId].getControl() == -1) {
    // Default control
    selectedControl[classId].setControl(oPunch::PunchFinish, previousControl);
  }

  gdi.popX();
}

int TabSpeaker::processListBox(gdioutput &gdi, const ListBoxInfo &bu)
{
  if (bu.id=="Leg") {
    if (classId>0) {
      selectedControl[classId].setLeg(bu.data>=1000, bu.data%1000);
      generateControlList(gdi, classId);
      gdi.setRestorePoint("speaker");
      gdi.setRestorePoint("SpeakerList");

      bool shortNames =  oe->getPropertyInt("SpeakerShortNames", false) != 0;
      oe->speakerList(gdi, classId, selectedControl[classId].getLeg(),
                      selectedControl[classId].getControl(),
                      selectedControl[classId].getPreviousControl(),
                      selectedControl[classId].isTotal(),
                      shortNames);
    }
  }
  else if (bu.id == "DetailLevel") {
    watchLevel = oTimeLine::Priority(bu.data);
    shownEvents.clear();
    events.clear();

    updateTimeLine(gdi);
  }
  else if (bu.id == "WatchNumber") {
    watchNumber = bu.data;
    updateTimeLine(gdi);
  }
  else if (bu.id == "Class") {
    savePriorityClass(gdi);
    int classId = int(bu.data);
    loadPriorityClass(gdi, classId);
  }
  return 0;
}

bool TabSpeaker::loadPage(gdioutput &gdi)
{
  oe->checkDB();

  gdi.clearPage(false);
  gdi.pushX();
  gdi.setRestorePoint("settings");

  gdi.pushX();
  gdi.fillDown();

  int h,w;
  gdi.getTargetDimension(w, h);

  int bw=gdi.scaleLength(100);
  int nbtn=max((w-80)/bw, 1);
  bw=(w-80)/nbtn;
  int basex = SPEAKER_BASE_X;
  int basey=gdi.getCY();

  int cx=basex;
  int cy=basey;
  int cb=1;
  for (set<int>::iterator it=classesToWatch.begin();it!=classesToWatch.end();++it) {
    char classid[32];
    sprintf_s(classid, "cid%d", *it);
    pClass pc=oe->getClass(*it);

    if (pc) {
      gdi.addButton(cx, cy, bw, classid, "#" + pc->getName(), tabSpeakerCB, "", false, false);
      cx+=bw;
      cb++;

      if (cb>nbtn) {
        cb=1;
        cx=basex;
        cy+=gdi.getButtonHeight()+4;
      }
    }
  }

  int db = 0;
  if (classesToWatch.empty()) {
    gdi.addString("", boldLarge, "Speakerst�d");
    gdi.dropLine();
    cy=gdi.getCY();
    cx=gdi.getCX();
  }
  else {
    gdi.addButton(cx+db, cy, bw-2, "Events", "H�ndelser", tabSpeakerCB, "L�pande information om viktiga h�ndelser i t�vlingen", false, false);
    if (++cb>nbtn) {
      cb = 1, cx = basex, db = 0;
      cy += gdi.getButtonHeight()+4;
    } else db += bw;
    gdi.addButton(cx+db, cy, bw/5, "ZoomIn", "+", tabSpeakerCB, "Zooma in (Ctrl + '+')", false, false);
    db += bw/5+2;
    gdi.addButton(cx+db, cy, bw/5, "ZoomOut", MakeDash("-"), tabSpeakerCB, "Zooma ut (Ctrl + '-')", false, false);
    db += bw/5+2;
  }
  gdi.addButton(cx+db, cy, bw-2, "Settings", "Inst�llningar...", tabSpeakerCB, "V�lj vilka klasser och kontroller som bevakas", false, false);
  if (++cb>nbtn) {
    cb = 1, cx = basex, db = 0;
    cy += gdi.getButtonHeight()+4;
  } else db += bw;
  gdi.addButton(cx+db, cy, bw-2, "Manual", "Tidsinmatning", tabSpeakerCB, "Mata in radiotider manuellt", false, false);
  if (++cb>nbtn) {
    cb = 1, cx = basex, db = 0;
    cy += gdi.getButtonHeight()+4;
  } else db += bw;

  gdi.addButton(cx+db, cy, bw-2, "PunchTable", "St�mplingar", tabSpeakerCB, "Visa en tabell �ver alla st�mplingar", false, false);
  if (++cb>nbtn) {
    cb = 1, cx = basex, db = 0;
    cy += gdi.getButtonHeight()+4;
  } else db += bw;

  gdi.addButton(cx+db, cy, bw-2, "LiveResult", "Liveresultat", tabSpeakerCB, "Visa rullande tider mellan kontroller i helsk�rmsl�ge", false, false);
  if (++cb>nbtn) {
    cb = 1, cx = basex, db = 0;
    cy += gdi.getButtonHeight()+4;
  } else db += bw;


  if (!ownWindow) {
    gdi.addButton(cx+db, cy, bw-2, "Priority", "Prioritering", tabSpeakerCB, "V�lj l�pare att prioritera bevakning f�r", false, false);
    if (++cb>nbtn) {
      cb = 1, cx = basex, db = 0;
      cy += gdi.getButtonHeight()+4;
    } else db += bw;


    gdi.addButton(cx+db, cy, bw-2, "Window", "Nytt f�nster", tabSpeakerCB, "", false, false);
    if (++cb>nbtn) {
      cb = 1, cx = basex, db = 0;
      cy += gdi.getButtonHeight()+4;
    } else db += bw;
  }
  gdi.setRestorePoint("classes");
  gdi.refresh();
  return true;
}

void TabSpeaker::clearCompetitionData()
{
  controlsToWatch.clear();
  classesToWatch.clear();
  controlsToWatchSI.clear();
  selectedControl.clear();
  classId=0;
  lastControl.clear();

  lastControlToWatch = 0;
  lastClassToWatch = 0;

  shownEvents.clear();
  events.clear();
}

void TabSpeaker::manualTimePage(gdioutput &gdi) const
{
  gdi.setRestorePoint("manual");

  gdi.fillRight();
  gdi.pushX();
  gdi.addInput("Control", lastControl, 5, 0, "Kontroll");
  gdi.addInput("Runner", "", 6, 0, "L�pare");
  gdi.addInput("Time", "", 8, 0, "Tid");
  gdi.dropLine();
  gdi.addButton("StoreTime", "Spara", tabSpeakerCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", tabSpeakerCB).setCancel();
  gdi.fillDown();
  gdi.popX();
  gdi.dropLine(3);
  gdi.addString("", 10, "help:14692");

  gdi.setInputFocus("Runner");
  gdi.refresh();
}

void TabSpeaker::storeManualTime(gdioutput &gdi)
{
  char bf[256];

  int punch=gdi.getTextNo("Control");

  if (punch<=0)
    throw std::exception("Kontrollnummer m�ste anges.");

  lastControl=gdi.getText("Control");
  const string &r_str=gdi.getText("Runner");
  string time=gdi.getText("Time");

 if (time.empty())
    time=getLocalTimeOnly();

  int itime=oe->getRelativeTime(time);

  if (itime <= 0)
    throw std::exception("Ogiltig tid.");

  pRunner r=oe->getRunnerByBibOrStartNo(r_str, false);
  int r_no = atoi(r_str.c_str());
  if (!r)
    r=oe->getRunnerByCardNo(r_no, itime);

  string Name;
  int sino=r_no;
  if (r) {
    Name=r->getName();
    sino=r->getCardNo();
  }
  else
    Name = lang.tl("Ok�nd");

  if (sino <= 0) {
    sprintf_s(bf, "Ogiltigt bricknummer.#%d", sino);
    throw std::exception(bf);
  }

  oe->addFreePunch(itime, punch, sino, true);

  gdi.restore("manual", false);
  gdi.addString("", 0, "L�pare: X, kontroll: Y, kl Z#" + Name + "#" + oPunch::getType(punch) + "#" +  oe->getAbsTime(itime));

  manualTimePage(gdi);
}

void TabSpeaker::loadPriorityClass(gdioutput &gdi, int classId) {

  gdi.restore("PrioList");
  gdi.setRestorePoint("PrioList");
  gdi.setOnClearCb(tabSpeakerCB);
  runnersToSet.clear();
  vector<pRunner> r;
  oe->getRunners(classId, 0, r);

  int x = gdi.getCX();
  int y = gdi.getCY()+2*gdi.getLineHeight();
  int dist = gdi.scaleLength(25);
  int dy = int(gdi.getLineHeight()*1.3);
  for (size_t k = 0; k<r.size(); k++) {
    if (r[k]->skip() /*|| r[k]->getLeg>0*/)
      continue;
    int pri = r[k]->getSpeakerPriority();
    int id = r[k]->getId();
    gdi.addCheckbox(x,y,"A" + itos(id), "", 0, pri>0);
    gdi.addCheckbox(x+dist,y,"B" + itos(id), "", 0, pri>1);
    gdi.addStringUT(y-dist/3, x+dist*2, 0, r[k]->getCompleteIdentification());
    runnersToSet.push_back(id);
    y += dy;
  }
  gdi.refresh();
}

void TabSpeaker::savePriorityClass(gdioutput &gdi) {
  oe->synchronizeList(oLRunnerId, true, false);
  oe->synchronizeList(oLTeamId, false, true);

  for (size_t k = 0; k<runnersToSet.size(); k++) {
    pRunner r = oe->getRunner(runnersToSet[k], 0);
    if (r) {
      int id = runnersToSet[k];
      if (!gdi.hasField("A" + itos(id))) {
        runnersToSet.clear(); //Page not loaded. Abort.
        return;
      }

      bool a = gdi.isChecked("A" + itos(id));
      bool b = gdi.isChecked("B" + itos(id));
      int pri = (a?1:0) + (b?1:0);
      pTeam t = r->getTeam();
      if (t) {
        t->setSpeakerPriority(pri);
        t->synchronize(true);
      }
      else {
        r->setSpeakerPriority(pri);
        r->synchronize(true);
      }
    }
  }
}

bool TabSpeaker::onClear(gdioutput &gdi) {
  if (!runnersToSet.empty())
    savePriorityClass(gdi);

  return true;
}