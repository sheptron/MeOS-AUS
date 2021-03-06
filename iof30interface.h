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

#pragma once
#include <map>
#include <set>
#include <vector>

class oEvent;
class xmlobject;
typedef vector<xmlobject> xmlList;
class xmlparser;
class gdioutput;
class oRunner;
class oClub;
class oTeam;
class oCourse;
class oControl;
class oClass;
class oDataInterface;
class oDataConstInterface;
struct RunnerDBEntry;
class RunnerDB;

typedef oRunner * pRunner;
typedef oClass * pClass;
typedef oClub * pClub;
typedef oTeam * pTeam;
typedef oCourse *pCourse;

class IOF30Interface {
  oEvent &oe;

  int cachedStageNumber;
  int entrySourceId;

  bool useGMT; // Use GMT when exporting

  // Export teams as individual
  bool teamsAsIndividual;
  // Unroll course loops
  bool unrollLoops;
  void operator=(const IOF30Interface &);

  struct LegInfo {
    int maxRunners;
    int minRunners;
    LegInfo() : maxRunners(1), minRunners(1) {}
    void setMinRunners(int nr) {minRunners = max(minRunners, nr); maxRunners = max(maxRunners, nr);}
    void setMaxRunners(int nr) {maxRunners = max(maxRunners, nr);}
  };

  struct FeeInfo {
    double fee;
    double taxable;
    double percentage; // Eventor / OLA stupidity

    string currency;

    string fromTime;
    string toTime;

    string fromBirthDate;
    string toBirthDate;

    bool includes(const FeeInfo &fo) const {
      if (toBirthDate != fo.toBirthDate || fromBirthDate != fo.fromBirthDate)
        return false;
      if (!includeFrom(fromTime, fo.fromTime))
        return false;
      if (!includeTo(toTime, fo.toTime))
        return false;
      /*if (!includeFrom(fromBirthDate, fo.fromBirthDate))
        return false;
      if (!includeTo(toBirthDate, fo.toBirthDate))
        return false;*/
      return true;
    }

    void add(FeeInfo &fi);

    string getDateKey() const {return fromTime + " - " + toTime;}
    FeeInfo() : fee(0), taxable(0), percentage(0) {}

    const bool operator<(const FeeInfo &fi) const {
      return fee < fi.fee || (fee == fi.fee && taxable < fi.taxable);
    }
  private:
    bool includeFrom(const string &a, const string &b) const {
      if ( a > b || (b.empty() && !a.empty()) )
        return false;
      return true;
    }

    bool includeTo(const string &a, const string &b) const {
      if ( (!a.empty() && a < b) || (b.empty() && !a.empty()) )
        return false;
      return true;
    }
  };

  struct FeeStatistics {
    double fee;
    double lateFactor;
  };

  vector<FeeStatistics> feeStatistics;

  static void getAgeLevels(const vector<FeeInfo> &fees, const vector<int> &ix,
                           int &normalIx, int &redIx, string &youthLimit, string &seniorLimit);

  void readEvent(gdioutput &gdi, const xmlobject &xo,
                 map<int, vector<LegInfo> > &teamClassConfig);
  pRunner readPersonEntry(gdioutput &gdi, xmlobject &xo, pTeam team,
                          const map<int, vector<LegInfo> > &teamClassConfig);
  pRunner readPerson(gdioutput &gdi, const xmlobject &xo);
  pClub readOrganization(gdioutput &gdi, const xmlobject &xo, bool saveToDB);
  pClass readClass(const xmlobject &xo,
                   map<int, vector<LegInfo> > &teamClassConfig);

  pTeam readTeamEntry(gdioutput &gdi, xmlobject &xTeam,
                      map<int, pair<string, int> > &bibPatterns,
                      const map<int, vector<LegInfo> > &teamClassConfig);

  pRunner readPersonStart(gdioutput &gdi, pClass pc, xmlobject &xo, pTeam team,
                          const map<int, vector<LegInfo> > &teamClassConfig);

  pTeam readTeamStart(gdioutput &gdi, pClass pc, xmlobject &xTeam,
                      map<int, pair<string, int> > &bibPatterns,
                      const map<int, vector<LegInfo> > &teamClassConfig);

  pTeam getCreateTeam(gdioutput &gdi, const xmlobject &xTeam, bool &newTeam);

  static int getIndexFromLegPos(int leg, int legorder, const vector<LegInfo> &setup);
  void setupClassConfig(int classId, const xmlobject &xTeam, map<int, vector<LegInfo> > &teamClassConfig);

  void setupRelayClasses(const map<int, vector<LegInfo> > &teamClassConfig);
  void setupRelayClass(pClass pc, const vector<LegInfo> &teamClassConfig);

  int parseISO8601Time(const xmlobject &xo);
  string getCurrentTime() const;

  static void getNationality(const xmlobject &xCountry, oDataInterface &di);

  static void getAmount(const xmlobject &xAmount, double &amount, string &currency);
  static void getAssignedFee(const xmlobject &xFee, double &fee, double &paid, double &taxable, double &percentage, string &currency);
  static void getFee(const xmlobject &xFee, FeeInfo &fee);
  static void getFeeAmounts(const xmlobject &xFee, double &fee, double &taxable, double &percentage, string &currency);

  void writeAmount(xmlparser &xml, const char *tag, int amount) const;
  void writeAssignedFee(xmlparser &xml, const oDataConstInterface &dci) const;
  void writeRentalCardService(xmlparser &xml, int cardFee) const;

  void getProps(vector<string> &props) const;

  void writeClassResult(xmlparser &xml, const oClass &c, const vector<pRunner> &r,
                        const vector<pTeam> &t);

  void writeClass(xmlparser &xml, const oClass &c);
  void writeCourse(xmlparser &xml, const oCourse &c);
  void writePersonResult(xmlparser &xml, const oRunner &r, bool includeCourse,
                         bool teamMember, bool hasInputTime);


  void writeTeamResult(xmlparser &xml, const oTeam &t, bool hasInputTime);

  void writeResult(xmlparser &xml, const oRunner &rPerson, const oRunner &rResultCarrier,
                   bool includeCourse, bool includeRaceNumber, bool teamMember, bool hasInputTime);

  void writePerson(xmlparser &xml, const oRunner &r);
  void writeClub(xmlparser &xml, const oClub &c, bool writeExtended) const;


  void getRunnersToUse(const pClass cls, vector<pRunner> &rToUse,
                       vector<pTeam> &tToUse, int leg, bool includeUnknown) const;

  void writeClassStartList(xmlparser &xml, const oClass &c, const vector<pRunner> &r,
                           const vector<pTeam> &t);

  void writePersonStart(xmlparser &xml, const oRunner &r, bool includeCourse, bool teamMember);

  void writeTeamStart(xmlparser &xml, const oTeam &t);

  void writeStart(xmlparser &xml, const oRunner &r, bool includeCourse,
                  bool includeRaceNumber, bool teamMember);


  pCourse haveSameCourse(const vector<pRunner> &r) const;
  void writeLegOrder(xmlparser &xml, const oRunner &r) const;

  // Returns zero if no stage number
  int getStageNumber();

  bool readXMLCompetitorDB(const xmlobject &xCompetitor);
  void writeXMLCompetitorDB(xmlparser &xml, const RunnerDBEntry &rde) const;

  int getStartIndex(const string &startId);

  bool readControl(const xmlobject &xControl);
  pCourse readCourse(const xmlobject &xcrs);

  void readCourseGroups(xmlobject xClassCourse, vector< vector<pCourse> > &crs);
  void bindClassCourse(oClass &pc, const vector< vector<pCourse> > &crs);

  static string constructCourseName(const xmlobject &xcrs);
  static string constructCourseName(const string &family, const string &name);

  void classAssignmentObsolete(gdioutput &gdi, xmlList &xAssignment, const map<string, pCourse> &courses,
                               const map<string, vector<pCourse> > &coursesFamilies);
  void classCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                             const map<string, pCourse> &courses,
                             const map<string, vector<pCourse> > &coursesFamilies);
  void personCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                              const map<string, pCourse> &courses);
  void teamCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                            const map<string, pCourse> &courses);

  void assignTeamCourse(gdioutput &gdi, oTeam &t, xmlList &xAssignment,
                        const map<string, pCourse> &courses);

  pCourse findCourse(gdioutput &gdi, const map<string, pCourse> &courses,
                     xmlobject &xPAssignment);

  string writeControl(xmlparser &xml, const oControl &c, set<string> &writtenId);

  void writeCourseInfo(xmlparser &xml, const oCourse &c);

  void writeFullCourse(xmlparser &xml, const oCourse &c,
                         const map<int, string> &ctrlId2ExportId);

public:
  IOF30Interface(oEvent *oe_) : oe(*oe_), useGMT(false), teamsAsIndividual(false), entrySourceId(1) {cachedStageNumber = -1;}
  virtual ~IOF30Interface() {}

  void readEventList(gdioutput &gdi, xmlobject &xo);

  void readEntryList(gdioutput &gdi, xmlobject &xo, bool removeNonexisting, int &entRead, int &entFail, int &entRemoved);

  void readStartList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail);

  void readClassList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail);

  void readCompetitorList(gdioutput &gdi, const xmlobject &xo, int &personCount);

  void readClubList(gdioutput &gdi, const xmlobject &xo, int &clubCount);

  void readCourseData(gdioutput &gdi, const xmlobject &xo, bool updateClasses, int &courseCount, int &entFail);

  void writeResultList(xmlparser &xml, const set<int> &classes, int leg,
                       bool useUTC, bool teamsAsIndividual, bool unrollLoops);

  void writeStartList(xmlparser &xml, const set<int> &classes, bool useUTC, bool teamsAsIndividual);

  void writeEvent(xmlparser &xml);

  void writeCourses(xmlparser &xml);

  void writeRunnerDB(const RunnerDB &db, xmlparser &xml) const;

  void writeClubDB(const RunnerDB &db, xmlparser &xml) const;
};
