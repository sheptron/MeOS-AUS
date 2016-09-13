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
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/
#include "stdafx.h"

#include <cassert>
#include "testmeos.h"

#include "gdioutput.h"
#include "meosexception.h"
#include "gdistructures.h" 
#include "meos_util.h"
#include "localizer.h"
#include "oEvent.h"
#include "TabSI.h"
#include "SportIdent.h"
#include "metalist.h"

void registerTests(TestMeOS &tm);

TestMeOS::TestMeOS(oEvent *oe, const string &test) : oe_main(oe),
                                                     gdi_main(&oe->gdibase), 
                                                     test(test), status(NORUN) {
  testId = 0;
  testIdMain = &testId;
  registerTests(*this);
  testId = 0;
}

TestMeOS::TestMeOS(TestMeOS &tmIn, const char *test) : oe_main(tmIn.oe_main),
                                                       gdi_main(tmIn.gdi_main),
                                                       test(test), status(NORUN) {
  testIdMain = 0;
  testId = 0;
}

TestMeOS::TestMeOS(const TestMeOS &tmIn, gdioutput &newWindow) : oe_main(tmIn.oe_main),
                                                                 gdi_main(&newWindow), 
                                                                 status(NORUN) {
  testIdMain = 0;
  testId = 0;
}
  

TestMeOS::~TestMeOS() {
  for (size_t k = 0; k < subTests.size(); k++) {
    delete subTests[k];
  }
  subTests.clear();
}

TestMeOS * TestMeOS::newInstance() const {
  throw meosException("Unsupported");
}

void TestMeOS::publishChildren(gdioutput &gdi, pair<int,int> &passFail) const {
  if (status == FAILED) {
    ++passFail.second;
    gdi.addStringUT(1, "FAILED " + test).setColor(colorRed);
    if (!message.empty())
      gdi.addStringUT(0, message);
  }
  else if (status == NORUN) {
    gdi.addStringUT(1, "DNS: " + test).setColor(colorDarkBlue);
  }
  else if (status == RUNNING) {
    gdi.addStringUT(1, "Running: " + test).setColor(colorDarkBlue);
  }
  else if (status == PASSED) {
    ++passFail.first;
    gdi.addStringUT(1, "PASSED " + test).setColor(colorGreen);
  }

  if (!subTests.empty()) {
    gdi.dropLine(0.5);
    int cx = gdi.getCX();
    gdi.setCX(cx + gdi.scaleLength(10));
    for (size_t j = 0; j < subTests.size(); j++) {
      subTests[j]->publishChildren(gdi, passFail);
    }
    gdi.setCX(cx);
  }
}

void TestMeOS::publish(gdioutput &gdi) const {
  gdi.clearPage(true);
  gdi.addStringUT(boldLarge, "Test Results");
  gdi.dropLine();
  pair<int, int> passFail;
  for (size_t j = 0; j < subTests.size(); j++) {
    subTests[j]->publishChildren(gdi, passFail);
  }

  gdi.dropLine();
  gdi.addString("", 1, "X tests failed, Y tests passed.#" + itos(passFail.second) + 
                                                      "#" + itos(passFail.first));

  gdi.refresh();
}

void TestMeOS::run() const {
/*  showTab(TClassTab); //Klasser
  press("Add"); //New Class
  press("MultiCourse"); //Several Courses / Relay...
  press("SetNStage"); //Confirm
  input("StartData0", "9");
  press("Save"); //Save
  showTab(TListTab); //Listor
  showTab(TTeamTab); //Lag(flera)
  press("Add"); //New Team
  input("R0", "A");
  input("R1", "B");
  input("R2", "C");
  input("SI0", "1001");
  input("SI1", "1002");
  input("SI2", "1001");
  press("Save"); //Save
  showTab(TRunnerTab); //Deltagare
  select("Runners", 1);
  select("Runners", 2);
  select("Runners", 3);
  select("Runners", 2);
  select("Runners", 1);
  select("Runners", 2);
  select("Runners", 3);
  select("Runners", 2);
  select("Runners", 1);
  showTab(TTeamTab); //Lag(flera)
  press("Add"); //New Team
  input("R0", "Ap");
  input("R1", "Bb");
  input("R2", "Cp");
  input("SI0", "1001");
  input("SI1", "1111");
  input("SI2", "1110");
  press("Save"); //Save
  showTab(TRunnerTab); //Deltagare
  select("Runners", 4);
  select("Runners", 3);
  select("Runners", 6);
  select("Runners", 1);
  select("Status", 1);
  input("Finish", "9:30");
  press("Save"); //Save
  select("Runners", 4);
  select("Runners", 6);
  select("Runners", 3);
  select("Runners", 2);
  input("Finish", "10:00");
  select("Status", 1);
  press("Save"); //Save
  select("Runners", 3);
  input("Finish", "10:30");
  select("Status", 1);
  press("Save"); //Save
  select("Runners", 1);
  select("Runners", 4);
  showTab(TCmpTab); //Tävling
  showTab(TRunnerTab); //Deltagare
  select("Runners", 1);
  select("Runners", 4);
  select("Runners", 1);
  showTab(TSITab); //SportIdent
  input("SI", "1001");
  press("Save"); //Card
  showTab(TRunnerTab); //Deltagare
  select("Runners", 4);
  select("Runners", 2);
  select("Runners", 4);
  select("Runners", 2);
  select("Runners", 4);
  input("CardNo", "10011");
  select("Runners", 3);
  select("Runners", 2);
  showTab(TSITab); //SportIdent
  input("SI", "1002");
  press("Save"); //Card
  showTab(TRunnerTab); //Deltagare
  select("Runners", 3);
  showTab(TSITab); //SportIdent
  input("SI", "1001");
  press("Save"); //Card
  showTab(TRunnerTab); //Deltagare
  select("Runners", 5);
  select("Runners", 2);
  select("Runners", 4);
  select("Runners", 1);
  select("Runners", 4);
  select("Runners", 2);
  select("Runners", 5);
  select("Runners", 3);
  select("Runners", 6);
  select("Runners", 3);
  select("Runners", 5);
  select("Runners", 2);
  select("Runners", 4);
  select("Runners", 1);
  select("Runners", 4);
  select("Runners", 2);
  select("Runners", 5);
  select("Runners", 3);
  select("Runners", 6);
  select("Runners", 3);
  select("Runners", 5);
  select("Runners", 2);
  select("Runners", 4);
  input("CardNo", "1001");
  press("Save"); //Save
  select("Runners", 2);
  select("Runners", 4);
  showTab(TCmpTab); //Tävling*/
}

void TestMeOS::runProtected(bool protect) const {
  cleanup();
  gdi_main->setOnClearCb(0);
  gdi_main->setPostClearCb(0);
  gdi_main->clearPage(false, false);
  subWindows.clear();
  oe_main->clear();
  gdi_main->isTestMode = true;
  showTab(TCmpTab);
  try {
    status = RUNNING;
    run();
    gdi_main->clearDialogAnswers(true);
    status = PASSED;
    if (protect) {
      gdi_main->setOnClearCb(0);
      gdi_main->setPostClearCb(0);
      gdi_main->clearPage(false, false);
      oe_main->clear();
      showTab(TCmpTab);
    }
    gdi_main->isTestMode = false;
  }
  catch (const meosAssertionFailure & ex) {
    status = FAILED;
    gdi_main->clearDialogAnswers(false);
    gdi_main->isTestMode = false;
    subWindows.clear();
    message = ex.message;
    if (!protect)
      throw meosException(message);
  }
  catch (const std::exception &ex) {
    status = FAILED;
    gdi_main->clearDialogAnswers(false);
    gdi_main->isTestMode = false;
    subWindows.clear();
    message = ex.what();
    if (!protect)
      throw;
  }
  catch (...) {
    status = FAILED;
    gdi_main->clearDialogAnswers(false);
    gdi_main->isTestMode = false;
    subWindows.clear();
    message = "Unknown Exception";
    cleanup();
    if (!protect)
      throw;
  }

  for (size_t k = 0; k < tmpFiles.size(); k++)
    removeTempFile(tmpFiles[k]);
  tmpFiles.clear();

  if (protect) {
    cleanup();
  }
}

void TestMeOS::runAll() const {
  runInternal(true);
}

void TestMeOS::runInternal(bool protect) const {
  runProtected(protect);
  for (size_t k = 0; k < subTests.size(); k++)
    subTests[k]->runInternal(protect);
}

bool TestMeOS::runSpecific(int id) const {
  if (id == testId) {
    runInternal(false);
    return true;
  }
  else {
    for (size_t k = 0; k < subTests.size(); k++) {
      if (subTests[k]->runSpecific(id))
        return true;
    }
  }
  return false;
}

void TestMeOS::getTests(vector< pair<string, size_t> > &tl) const {
  tl.push_back(make_pair(test, testId));
  for (size_t k = 0; k < subTests.size(); k++) {
    subTests[k]->getTests(tl);    
  }
}

TestMeOS &TestMeOS::registerTest(const TestMeOS &test) {
  subTests.push_back(test.newInstance());
  subTests.back()->testId = ++(*testIdMain);
  subTests.back()->testIdMain = testIdMain;
  return *subTests.back();
}

void mainMessageLoop(HACCEL hAccelTable, DWORD time);

void TestMeOS::showTab(TabType type) const {
  if (gdi_main->canClear()) {
    gdi_main->getTabs().get(type)->loadPage(*gdi_main);
    mainMessageLoop(0, 50);
  }
}

void TestMeOS::press(const char *btn) const {
  gdi_main->dbPress(btn, -65536);
  mainMessageLoop(0, 50);
}

void TestMeOS::press(const char *btn, int extra) const {
  gdi_main->dbPress(btn, extra);
  mainMessageLoop(0, 50);
}

void TestMeOS::press(const char *btn, const char *extra) const {
  gdi_main->dbPress(btn, extra);
  mainMessageLoop(0, 50);
}

string TestMeOS::select(const char *id, size_t data) const {
  string res = gdi_main->dbSelect(id, data);
  mainMessageLoop(0, 50);
  return res;
}

void TestMeOS::dblclick(const char *id, size_t data) const {
  gdi_main->dbDblClick(id, data);
}

void TestMeOS::input(const char *id, const char *data) const {
  string arg;
  while(*data) {
    if (*data == '\n') {
      arg.push_back('\r');
      arg.push_back('\n');
    }
    else {
      arg.push_back(*data);
    }
    data++;
  }
  gdi_main->dbInput(id, arg);
  mainMessageLoop(0, 50);
}

void TestMeOS::click(const char *id) const {
  gdi_main->dbClick(id, -65536);
  mainMessageLoop(0, 50);
}

void TestMeOS::click(const char *id, int extra) const {
  gdi_main->dbClick(id, extra);
  mainMessageLoop(0, 50);
}


string TestMeOS::getText(const char *ctrl) const {
  return gdi_main->getText(ctrl, false);
}

bool TestMeOS::isChecked(const char *ctrl) const {
  return gdi_main->isChecked(ctrl);
}

void TestMeOS::assertEquals(const string &expected,
                            const string &value) const {
  if (expected != value)
    throw meosAssertionFailure("Expected " + expected + " but got " + value);
}

void TestMeOS::assertEquals(int expected, int value) const {
  assertEquals(itos(expected), itos(value));
}
  
void TestMeOS::assertTrue(const char *message, bool condition) const {
  assertEquals(message, "true", condition ? "true" : "false");
}

void TestMeOS::assertEquals(const string &message, 
                            const string &expected,
                            const string &value) const {
  if (expected != value)
    throw meosAssertionFailure(message +  ": Expected " + expected + " but got " + value);
}

void TestMeOS::assertEquals(const char *message, 
                            const char *expected,
                            const string &value) const {
  assertEquals(string(message), string(expected), value);
}

void TestMeOS::checkString(const char *str, int count) const {
  int c = gdi_main->dbGetStringCount(str);
  assertEquals("String " + string(str) + " not found", itos(count), itos(c));
}

void TestMeOS::checkStringRes(const char *str, int count) const {
  int c = gdi_main->dbGetStringCount(lang.tl(str));
  assertEquals("String " + string(str) + " not found", itos(count), itos(c));
}


void TestMeOS::insertCard(int cardNo, const char *ser) const {
  SICard sic;
  sic.CardNumber = cardNo;
  sic.deserializePunches(ser);
  TabSI::getSI(*gdi_main).addCard(sic);
  mainMessageLoop(0, 100);
}

void TestMeOS::setAnswer(const char *ans) const {
  gdi_main->dbPushDialogAnswer(ans);
}

void TestMeOS::setFile(const string &file) const {
  gdi_main->dbPushDialogAnswer("*" + file);
}

void TestMeOS::cleanup() const {
}

int TestMeOS::getResultModuleIndex(const char *tag) const {
  vector< pair<string, string> > mol;
  oe_main->getGeneralResults(false, mol, true);
  for (size_t k = 0; k < mol.size(); k++) {
    if (mol[k].first == tag) {
      return k + 100;
    }
  }
  throw meosException(string("Result module not found: ") + tag);
}

int TestMeOS::getListIndex(const char *name) const {
  vector< pair<string, size_t> > lst;
  oe_main->getListContainer().getLists(lst, false, false, false);
  for (size_t k = 0; k < lst.size(); k++) {
    if (lst[k].first == name)
      return lst[k].second;
  }
  throw meosException(string("List not found: ") + name);
}

const string &getLastExtraWindow();

const string TestMeOS::getLastExtraWindow() const {
  return ::getLastExtraWindow(); 
}

const TestMeOS &TestMeOS::getExtraWindow(const string &tag) const {
  gdioutput *ge = ::getExtraWindow(tag, false);
  if (ge == 0)
    throw meosException(string("Window not found: ") + tag);

  subWindows.push_back(TestMeOS(*this, *ge));
  return subWindows.back();
}

void TestMeOS::pressEscape() const {
  gdi_main->doEscape();
}

void TestMeOS::closeWindow() const {
  gdi_main->closeWindow();
}

string TestMeOS::getTestFile(const char *relPath) const {
  string tp = oe_main->getPropertyString("TestPath", "");
  if (tp.length() == 0)
    return relPath;
  else if (*tp.rbegin() == '\\')
    return tp + relPath;
  else
    return tp + "\\" + relPath;
}

string TestMeOS::getTempFile() const {
  string fn = ::getTempFile();
  tmpFiles.push_back(fn);
  return fn;
}
