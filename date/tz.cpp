#pragma once
//{{{
// The MIT License (MIT)
//
// Copyright (c) 2015, 2016, 2017 Howard Hinnant
// Copyright (c) 2015 Ville Voutilainen
// Copyright (c) 2016 Alexander Kormanovsky
// Copyright (c) 2016, 2017 Jiangang Zhuang
// Copyright (c) 2017 Nicolas Veloz Savino
// Copyright (c) 2017 Florian Dang
// Copyright (c) 2017 Aaron Bishop
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Our apologies.  When the previous paragraph was written, lowercase had not yet
// been invented (that would involve another several millennia of evolution).
// We did not mean to shout.
//}}}
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

#include "tz_private.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <sys/stat.h>

#include <io.h> // _unlink etc.
#include <shlobj.h> // CoTaskFree, ShGetKnownFolderPath etc.

static CONSTDATA char folder_delimiter = '\\';
//}}}

namespace date {
  using namespace detail;
  //{{{  reduce the range of the database to save memory
  CONSTDATA auto min_year = date::year::min();
  CONSTDATA auto max_year = date::year::max();
  CONSTDATA auto min_day = date::jan/1;
  CONSTDATA auto max_day = date::dec/31;
  //}}}
  namespace detail {
    struct undocumented {explicit undocumented() = default;};
    }
  //{{{
  static const std::string& get_install() {
    static const std::string& ref = "c:/projects/D2dWindow/date/tzdata";
    return ref;
    }
  //}}}

  // tzdb
  static std::unique_ptr<tzdb> init_tzdb();
  //{{{
  tzdb_list::~tzdb_list()
  {
      const tzdb* ptr = head_;
      head_ = nullptr;
      while (ptr != nullptr)
      {
          auto next = ptr->next;
          delete ptr;
          ptr = next;
      }
  }
  //}}}
  //{{{
  tzdb_list::tzdb_list (tzdb_list&& x) noexcept
     : head_{x.head_.exchange(nullptr)}
  {
  }
  //}}}
  //{{{
  void tzdb_list::push_front (tzdb* tzdb) noexcept
  {
      tzdb->next = head_;
      head_ = tzdb;
  }
  //}}}
  //{{{
  tzdb_list::const_iterator tzdb_list::erase_after (const_iterator p) noexcept
  {
      auto t = p.p_->next;
      p.p_->next = p.p_->next->next;
      delete t;
      return ++p;
  }
  //}}}
  //{{{
  struct tzdb_list::undocumented_helper
  {
      static void push_front(tzdb_list& db_list, tzdb* tzdb) noexcept
      {
          db_list.push_front(tzdb);
      }
  };
  //}}}
  //{{{
  static tzdb_list create_tzdb()
  {
      tzdb_list tz_db;
      tzdb_list::undocumented_helper::push_front(tz_db, init_tzdb().release());
      return tz_db;
  }
  //}}}
  //{{{
  tzdb_list& get_tzdb_list()
  {
      static tzdb_list tz_db = create_tzdb();
      return tz_db;
  }
  //}}}

  //{{{
  static void sort_zone_mappings (std::vector<date::detail::timezone_mapping>& mappings)
  {
      std::sort(mappings.begin(), mappings.end(),
          [](const date::detail::timezone_mapping& lhs,
             const date::detail::timezone_mapping& rhs)->bool
      {
          auto other_result = lhs.other.compare(rhs.other);
          if (other_result < 0)
              return true;
          else if (other_result == 0)
          {
              auto territory_result = lhs.territory.compare(rhs.territory);
              if (territory_result < 0)
                  return true;
              else if (territory_result == 0)
              {
                  if (lhs.type < rhs.type)
                      return true;
              }
          }
          return false;
      });
  }
  //}}}
  //{{{
  static bool native_to_standard_timezone_name (const std::string& native_tz_name,
                                   std::string& standard_tz_name)
  {
      // TOOD! Need be a case insensitive compare?
      if (native_tz_name == "UTC")
      {
          standard_tz_name = "Etc/UTC";
          return true;
      }
      standard_tz_name.clear();
      // TODO! we can improve on linear search.
      const auto& mappings = date::get_tzdb().mappings;
      for (const auto& tzm : mappings)
      {
          if (tzm.other == native_tz_name)
          {
              standard_tz_name = tzm.type;
              return true;
          }
      }
      return false;
  }
  //}}}
  //{{{
  // Parse this XML file:
  // http://unicode.org/repos/cldr/trunk/common/supplemental/windowsZones.xml
  // The parsing method is designed to be simple and quick. It is not overly
  // forgiving of change but it should diagnose basic format issues.
  // See timezone_mapping structure for more info.
  static std::vector<detail::timezone_mapping> load_timezone_mappings_from_xml_file (const std::string& input_path)
  {
      std::size_t line_num = 0;
      std::vector<detail::timezone_mapping> mappings;
      std::string line;

      std::ifstream is(input_path);
      if (!is.is_open())
      {
          // We don't emit file exceptions because that's an implementation detail.
          std::string msg = "Error opening time zone mapping file \"";
          msg += input_path;
          msg += "\".";
          throw std::runtime_error(msg);
      }

      auto error = [&input_path, &line_num](const char* info)
      {
          std::string msg = "Error loading time zone mapping file \"";
          msg += input_path;
          msg += "\" at line ";
          msg += std::to_string(line_num);
          msg += ": ";
          msg += info;
          throw std::runtime_error(msg);
      };
      // [optional space]a="b"
      auto read_attribute = [&line_num, &line, &error]
                            (const char* name, std::string& value, std::size_t startPos)
                            ->std::size_t
      {
          value.clear();
          // Skip leading space before attribute name.
          std::size_t spos = line.find_first_not_of(' ', startPos);
          if (spos == std::string::npos)
              spos = startPos;
          // Assume everything up to next = is the attribute name
          // and that an = will always delimit that.
          std::size_t epos = line.find('=', spos);
          if (epos == std::string::npos)
              error("Expected \'=\' right after attribute name.");
          std::size_t name_len = epos - spos;
          // Expect the name we find matches the name we expect.
          if (line.compare(spos, name_len, name) != 0)
          {
              std::string msg;
              msg = "Expected attribute name \'";
              msg += name;
              msg += "\' around position ";
              msg += std::to_string(spos);
              msg += " but found something else.";
              error(msg.c_str());
          }
          ++epos; // Skip the '=' that is after the attribute name.
          spos = epos;
          if (spos < line.length() && line[spos] == '\"')
              ++spos; // Skip the quote that is before the attribute value.
          else
          {
              std::string msg = "Expected '\"' to begin value of attribute \'";
              msg += name;
              msg += "\'.";
              error(msg.c_str());
          }
          epos = line.find('\"', spos);
          if (epos == std::string::npos)
          {
              std::string msg = "Expected '\"' to end value of attribute \'";
              msg += name;
              msg += "\'.";
              error(msg.c_str());
          }
          // Extract everything in between the quotes. Note no escaping is done.
          std::size_t value_len = epos - spos;
          value.assign(line, spos, value_len);
          ++epos; // Skip the quote that is after the attribute value;
          return epos;
      };

      // Quick but not overly forgiving XML mapping file processing.
      bool mapTimezonesOpenTagFound = false;
      bool mapTimezonesCloseTagFound = false;
      std::size_t mapZonePos = std::string::npos;
      std::size_t mapTimezonesPos = std::string::npos;
      CONSTDATA char mapTimeZonesOpeningTag[] = { "<mapTimezones " };
      CONSTDATA char mapZoneOpeningTag[] = { "<mapZone " };
      CONSTDATA std::size_t mapZoneOpeningTagLen = sizeof(mapZoneOpeningTag) /
                                                   sizeof(mapZoneOpeningTag[0]) - 1;
      while (!mapTimezonesOpenTagFound)
      {
          std::getline(is, line);
          ++line_num;
          if (is.eof())
          {
              // If there is no mapTimezones tag is it an error?
              // Perhaps if there are no mapZone mappings it might be ok for
              // its parent mapTimezones element to be missing?
              // We treat this as an error though on the assumption that if there
              // really are no mappings we should still get a mapTimezones parent
              // element but no mapZone elements inside. Assuming we must
              // find something will hopefully at least catch more drastic formatting
              // changes or errors than if we don't do this and assume nothing found.
              error("Expected a mapTimezones opening tag.");
          }
          mapTimezonesPos = line.find(mapTimeZonesOpeningTag);
          mapTimezonesOpenTagFound = (mapTimezonesPos != std::string::npos);
      }

      // NOTE: We could extract the version info that follows the opening
      // mapTimezones tag and compare that to the version of other data we have.
      // I would have expected them to be kept in synch but testing has shown
      // it is typically does not match anyway. So what's the point?
      while (!mapTimezonesCloseTagFound)
      {
          std::ws(is);
          std::getline(is, line);
          ++line_num;
          if (is.eof())
              error("Expected a mapTimezones closing tag.");
          if (line.empty())
              continue;
          mapZonePos = line.find(mapZoneOpeningTag);
          if (mapZonePos != std::string::npos)
          {
              mapZonePos += mapZoneOpeningTagLen;
              detail::timezone_mapping zm{};
              std::size_t pos = read_attribute("other", zm.other, mapZonePos);
              pos = read_attribute("territory", zm.territory, pos);
              read_attribute("type", zm.type, pos);
              mappings.push_back(std::move(zm));

              continue;
          }
          mapTimezonesPos = line.find("</mapTimezones>");
          mapTimezonesCloseTagFound = (mapTimezonesPos != std::string::npos);
          if (!mapTimezonesCloseTagFound)
          {
              std::size_t commentPos = line.find("<!--");
              if (commentPos == std::string::npos)
                  error("Unexpected mapping record found. A xml mapZone or comment "
                        "attribute or mapTimezones closing tag was expected.");
          }
      }

      is.close();
      return mappings;
  }
  //}}}

  //{{{
  link::link (const std::string& s)
  {
      using namespace date;
      std::istringstream in(s);
      in.exceptions(std::ios::failbit | std::ios::badbit);
      std::string word;
      in >> word >> target_ >> name_;
  }
  //}}}
  //{{{
  std::ostream& operator << (std::ostream& os, const link& x)
  {
      using namespace date;
      detail::save_stream<char> _(os);
      os.fill(' ');
      os.flags(std::ios::dec | std::ios::left);
      os.width(35);
      return os << x.name_ << " --> " << x.target_;
  }
  //}}}
  //{{{
  leap::leap(const std::string& s, detail::undocumented)
  {
      using namespace date;
      std::istringstream in(s);
      in.exceptions(std::ios::failbit | std::ios::badbit);
      std::string word;
      int y;
      MonthDayTime date;
      in >> word >> y >> date;
      date_ = date.to_time_point(year(y));
  }
  //}}}
  //{{{
  static bool file_exists (const std::string& filename)
  {
      return ::_access(filename.c_str(), 0) == 0;
  }
  //}}}
  //{{{
  static std::string get_version (const std::string& path)
  {
      std::string version;
      std::ifstream infile(path + "version");
      if (infile.is_open())
      {
          infile >> version;
          if (!infile.fail())
              return version;
      }
      else
      {
          infile.open(path + "NEWS");
          while (infile)
          {
              infile >> version;
              if (version == "Release")
              {
                  infile >> version;
                  return version;
              }
          }
      }
      throw std::runtime_error("Unable to get Timezone database version from " + path);
  }
  //}}}

  //{{{
  static std::unique_ptr<tzdb> init_tzdb()
  {
      using namespace date;
      const std::string install = get_install();
      const std::string path = install + folder_delimiter;
      std::string line;
      bool continue_zone = false;
      std::unique_ptr<tzdb> db(new tzdb);

      if (!file_exists(install))
      {
          std::string msg = "Timezone database not found at \"";
          msg += install;
          msg += "\"";
          throw std::runtime_error(msg);
      }
      db->version = get_version(path);

      CONSTDATA char*const files[] =
      {
          "africa", "antarctica", "asia", "australasia", "backward", "etcetera", "europe",
          "pacificnew", "northamerica", "southamerica", "systemv", "leapseconds"
      };

      for (const auto& filename : files)
      {
          std::ifstream infile(path + filename);
          while (infile)
          {
              std::getline(infile, line);
              if (!line.empty() && line[0] != '#')
              {
                  std::istringstream in(line);
                  std::string word;
                  in >> word;
                  if (word == "Rule")
                  {
                      db->rules.push_back(Rule(line));
                      continue_zone = false;
                  }
                  else if (word == "Link")
                  {
                      db->links.push_back(link(line));
                      continue_zone = false;
                  }
                  else if (word == "Leap")
                  {
                      db->leaps.push_back(leap(line, detail::undocumented{}));
                      continue_zone = false;
                  }
                  else if (word == "Zone")
                  {
                      db->zones.push_back(time_zone(line, detail::undocumented{}));
                      continue_zone = true;
                  }
                  else if (line[0] == '\t' && continue_zone)
                  {
                      db->zones.back().add(line);
                  }
                  else
                  {
                      std::cerr << line << '\n';
                  }
              }
          }
      }
      std::sort(db->rules.begin(), db->rules.end());
      Rule::split_overlaps(db->rules);
      std::sort(db->zones.begin(), db->zones.end());
      db->zones.shrink_to_fit();
      std::sort(db->links.begin(), db->links.end());
      db->links.shrink_to_fit();
      std::sort(db->leaps.begin(), db->leaps.end());
      db->leaps.shrink_to_fit();

      std::string mapping_file = get_install() + folder_delimiter + "windowsZones.xml";
      db->mappings = load_timezone_mappings_from_xml_file(mapping_file);
      sort_zone_mappings(db->mappings);

      return db;
  }
  //}}}
  //{{{
  const tzdb& reload_tzdb()
  {
  #if AUTO_DOWNLOAD
      auto const& v = get_tzdb_list().front().version;
      if (!v.empty() && v == remote_version())
          return get_tzdb_list().front();
  #endif  // AUTO_DOWNLOAD
      tzdb_list::undocumented_helper::push_front(get_tzdb_list(), init_tzdb().release());
      return get_tzdb_list().front();
  }
  //}}}
  //{{{
  const tzdb& get_tzdb()
  {
      return get_tzdb_list().front();
  }
  //}}}
  //{{{
  const time_zone* tzdb::locate_zone (const std::string& tz_name) const
  {
      auto zi = std::lower_bound(zones.begin(), zones.end(), tz_name,
          [](const time_zone& z, const std::string& nm)
          {
              return z.name() < nm;
          });
      if (zi == zones.end() || zi->name() != tz_name)
      {
          auto li = std::lower_bound(links.begin(), links.end(), tz_name,
          [](const link& z, const std::string& nm)
          {
              return z.name() < nm;
          });
          if (li != links.end() && li->name() == tz_name)
          {
              zi = std::lower_bound(zones.begin(), zones.end(), li->target(),
                  [](const time_zone& z, const std::string& nm)
                  {
                      return z.name() < nm;
                  });
              if (zi != zones.end() && zi->name() == li->target())
                  return &*zi;
          }
          throw std::runtime_error(std::string(tz_name) + " not found in timezone database");
      }
      return &*zi;
  }
  //}}}
  //{{{
  const time_zone* locate_zone (const std::string& tz_name)
  {
      return get_tzdb().locate_zone(tz_name);
  }
  //}}}
  //{{{
  std::ostream& operator << (std::ostream& os, const tzdb& db)
  {
      os << "Version: " << db.version << '\n';
      std::string title("--------------------------------------------"
                        "--------------------------------------------\n"
                        "Name           ""Start Y ""End Y   "
                        "Beginning                              ""Offset  "
                        "Designator\n"
                        "--------------------------------------------"
                        "--------------------------------------------\n");
      int count = 0;
      for (const auto& x : db.rules)
      {
          if (count++ % 50 == 0)
              os << title;
          os << x << '\n';
      }
      os << '\n';
      title = std::string("---------------------------------------------------------"
                          "--------------------------------------------------------\n"
                          "Name                               ""Offset      "
                          "Rule           ""Abrev      ""Until\n"
                          "---------------------------------------------------------"
                          "--------------------------------------------------------\n");
      count = 0;
      for (const auto& x : db.zones)
      {
          if (count++ % 10 == 0)
              os << title;
          os << x << '\n';
      }
      os << '\n';
      title = std::string("---------------------------------------------------------"
                          "--------------------------------------------------------\n"
                          "Alias                                   ""To\n"
                          "---------------------------------------------------------"
                          "--------------------------------------------------------\n");
      count = 0;
      for (const auto& x : db.links)
      {
          if (count++ % 45 == 0)
              os << title;
          os << x << '\n';
      }
      os << '\n';
      title = std::string("---------------------------------------------------------"
                          "--------------------------------------------------------\n"
                          "Leap second on\n"
                          "---------------------------------------------------------"
                          "--------------------------------------------------------\n");
      os << title;
      for (const auto& x : db.leaps)
          os << x << '\n';
      return os;
  }
  //}}}
  //{{{
  static std::string getTimeZoneKeyName()
  {
      DYNAMIC_TIME_ZONE_INFORMATION dtzi{};
      auto result = GetDynamicTimeZoneInformation(&dtzi);
      if (result == TIME_ZONE_ID_INVALID)
          throw std::runtime_error("current_zone(): GetDynamicTimeZoneInformation()"
                                   " reported TIME_ZONE_ID_INVALID.");
      auto wlen = wcslen(dtzi.TimeZoneKeyName);
      char buf[128] = {};
      assert(sizeof(buf) >= wlen+1);
      wcstombs(buf, dtzi.TimeZoneKeyName, wlen);
      if (strcmp(buf, "Coordinated Universal Time") == 0)
          return "UTC";
      return buf;
  }
  //}}}
  //{{{
  const time_zone* tzdb::current_zone() const
  {
      std::string win_tzid = getTimeZoneKeyName();
      std::string standard_tzid;
      if (!native_to_standard_timezone_name(win_tzid, standard_tzid))
      {
          std::string msg;
          msg = "current_zone() failed: A mapping from the Windows Time Zone id \"";
          msg += win_tzid;
          msg += "\" was not found in the time zone mapping database.";
          throw std::runtime_error(msg);
      }
      return locate_zone(standard_tzid);
  }
  //}}}
  //{{{
  const time_zone* current_zone()
  {
      return get_tzdb().current_zone();
  }
  //}}}

  // Parsing helpers
  //{{{
  static std::string parse3 (std::istream& in)
  {
      std::string r(3, ' ');
      ws(in);
      r[0] = static_cast<char>(in.get());
      r[1] = static_cast<char>(in.get());
      r[2] = static_cast<char>(in.get());
      return r;
  }
  //}}}
  //{{{
  static unsigned parse_dow (std::istream& in) {
      CONSTDATA char*const dow_names[] =
          {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
      auto s = parse3(in);
      auto dow = std::find(std::begin(dow_names), std::end(dow_names), s) - dow_names;
      if (dow >= std::end(dow_names) - std::begin(dow_names))
          throw std::runtime_error("oops: bad dow name: " + s);
      return static_cast<unsigned>(dow);
  }
  //}}}
  //{{{
  static unsigned parse_month (std::istream& in) {
      CONSTDATA char*const month_names[] =
          {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
      auto s = parse3(in);
      auto m = std::find(std::begin(month_names), std::end(month_names), s) - month_names;
      if (m >= std::end(month_names) - std::begin(month_names))
          throw std::runtime_error("oops: bad month name: " + s);
      return static_cast<unsigned>(++m);
  }
  //}}}
  //{{{
  static std::chrono::seconds parse_unsigned_time (std::istream& in)
  {
      using namespace std::chrono;
      int x;
      in >> x;
      auto r = seconds{hours{x}};
      if (!in.eof() && in.peek() == ':')
      {
          in.get();
          in >> x;
          r += minutes{x};
          if (!in.eof() && in.peek() == ':')
          {
              in.get();
              in >> x;
              r += seconds{x};
          }
      }
      return r;
  }
  //}}}
  //{{{
  static std::chrono::seconds parse_signed_time (std::istream& in)
  {
      ws(in);
      auto sign = 1;
      if (in.peek() == '-')
      {
          sign = -1;
          in.get();
      }
      else if (in.peek() == '+')
          in.get();
      return sign * parse_unsigned_time(in);
  }
  //}}}
  //{{{
  // MonthDayTime

  detail::MonthDayTime::MonthDayTime (local_seconds tp, tz timezone)
      : zone_(timezone)
  {
      using namespace date;
      const auto dp = date::floor<days>(tp);
      const auto hms = make_time(tp - dp);
      const auto ymd = year_month_day(dp);
      u = ymd.month() / ymd.day();
      h_ = hms.hours();
      m_ = hms.minutes();
      s_ = hms.seconds();
  }
  //}}}
  //{{{
  detail::MonthDayTime::MonthDayTime (const date::month_day& md, tz timezone)
      : zone_(timezone)
  {
      u = md;
  }
  //}}}
  //{{{
  date::day detail::MonthDayTime::day() const
  {
      switch (type_)
      {
      case month_day:
          return u.month_day_.day();
      case month_last_dow:
          return date::day{31};
      case lteq:
      case gteq:
          break;
      }
      return u.month_day_weekday_.month_day_.day();
  }
  //}}}
  //{{{
  date::month detail::MonthDayTime::month() const
  {
      switch (type_)
      {
      case month_day:
          return u.month_day_.month();
      case month_last_dow:
          return u.month_weekday_last_.month();
      case lteq:
      case gteq:
          break;
      }
      return u.month_day_weekday_.month_day_.month();
  }
  //}}}
  //{{{
  int detail::MonthDayTime::compare (date::year y, const MonthDayTime& x, date::year yx,
                        std::chrono::seconds offset, std::chrono::minutes prev_save) const
  {
      if (zone_ != x.zone_)
      {
          auto dp0 = to_sys_days(y);
          auto dp1 = x.to_sys_days(yx);
          if (std::abs((dp0-dp1).count()) > 1)
              return dp0 < dp1 ? -1 : 1;
          if (zone_ == tz::local)
          {
              auto tp0 = to_time_point(y) - prev_save;
              if (x.zone_ == tz::utc)
                  tp0 -= offset;
              auto tp1 = x.to_time_point(yx);
              return tp0 < tp1 ? -1 : tp0 == tp1 ? 0 : 1;
          }
          else if (zone_ == tz::standard)
          {
              auto tp0 = to_time_point(y);
              auto tp1 = x.to_time_point(yx);
              if (x.zone_ == tz::local)
                  tp1 -= prev_save;
              else
                  tp0 -= offset;
              return tp0 < tp1 ? -1 : tp0 == tp1 ? 0 : 1;
          }
          // zone_ == tz::utc
          auto tp0 = to_time_point(y);
          auto tp1 = x.to_time_point(yx);
          if (x.zone_ == tz::local)
              tp1 -= offset + prev_save;
          else
              tp1 -= offset;
          return tp0 < tp1 ? -1 : tp0 == tp1 ? 0 : 1;
      }
      auto const t0 = to_time_point(y);
      auto const t1 = x.to_time_point(yx);
      return t0 < t1 ? -1 : t0 == t1 ? 0 : 1;
  }
  //}}}
  //{{{
  sys_seconds detail::MonthDayTime::to_sys (date::year y, std::chrono::seconds offset,
                       std::chrono::seconds save) const
  {
      using namespace date;
      using namespace std::chrono;
      auto until_utc = to_time_point(y);
      if (zone_ == tz::standard)
          until_utc -= offset;
      else if (zone_ == tz::local)
          until_utc -= offset + save;
      return until_utc;
  }
  //}}}
  //{{{
  detail::MonthDayTime::U& detail::MonthDayTime::U::operator = (const date::month_day& x)
  {
      month_day_ = x;
      return *this;
  }
  //}}}
  //{{{
  detail::MonthDayTime::U& detail::MonthDayTime::U::operator = (const date::month_weekday_last& x)
  {
      month_weekday_last_ = x;
      return *this;
  }
  //}}}
  //{{{
  detail::MonthDayTime::U& detail::MonthDayTime::U::operator = (const pair& x)
  {
      month_day_weekday_ = x;
      return *this;
  }
  //}}}
  //{{{
  date::sys_days detail::MonthDayTime::to_sys_days (date::year y) const
  {
      using namespace std::chrono;
      using namespace date;
      switch (type_)
      {
      case month_day:
          return sys_days(y/u.month_day_);
      case month_last_dow:
          return sys_days(y/u.month_weekday_last_);
      case lteq:
          {
              auto const x = y/u.month_day_weekday_.month_day_;
              auto const wd1 = weekday(static_cast<sys_days>(x));
              auto const wd0 = u.month_day_weekday_.weekday_;
              return sys_days(x) - (wd1-wd0);
          }
      case gteq:
          break;
      }
      auto const x = y/u.month_day_weekday_.month_day_;
      auto const wd1 = u.month_day_weekday_.weekday_;
      auto const wd0 = weekday(static_cast<sys_days>(x));
      return sys_days(x) + (wd1-wd0);
  }
  //}}}

  //{{{
  sys_seconds detail::MonthDayTime::to_time_point (date::year y) const
  {
      // Add seconds first to promote to largest rep early to prevent overflow
      return to_sys_days(y) + s_ + h_ + m_;
  }
  //}}}
  //{{{
  void detail::MonthDayTime::canonicalize (date::year y)
  {
      using namespace std::chrono;
      using namespace date;
      switch (type_)
      {
      case month_day:
          return;
      case month_last_dow:
          {
              auto const ymd = year_month_day(sys_days(y/u.month_weekday_last_));
              u.month_day_ = ymd.month()/ymd.day();
              type_ = month_day;
              return;
          }
      case lteq:
          {
              auto const x = y/u.month_day_weekday_.month_day_;
              auto const wd1 = weekday(static_cast<sys_days>(x));
              auto const wd0 = u.month_day_weekday_.weekday_;
              auto const ymd = year_month_day(sys_days(x) - (wd1-wd0));
              u.month_day_ = ymd.month()/ymd.day();
              type_ = month_day;
              return;
          }
      case gteq:
          {
              auto const x = y/u.month_day_weekday_.month_day_;
              auto const wd1 = u.month_day_weekday_.weekday_;
              auto const wd0 = weekday(static_cast<sys_days>(x));
              auto const ymd = year_month_day(sys_days(x) + (wd1-wd0));
              u.month_day_ = ymd.month()/ymd.day();
              type_ = month_day;
              return;
          }
      }
  }
  //}}}
  //{{{
  std::istream& detail::operator >> (std::istream& is, MonthDayTime& x)
  {
      using namespace date;
      using namespace std::chrono;
      assert(((std::ios::failbit | std::ios::badbit) & is.exceptions()) ==
              (std::ios::failbit | std::ios::badbit));
      x = MonthDayTime{};
      if (!is.eof() && ws(is) && !is.eof() && is.peek() != '#')
      {
          auto m = parse_month(is);
          if (!is.eof() && ws(is) && !is.eof() && is.peek() != '#')
          {
              if (is.peek() == 'l')
              {
                  for (int i = 0; i < 4; ++i)
                      is.get();
                  auto dow = parse_dow(is);
                  x.type_ = MonthDayTime::month_last_dow;
                  x.u = date::month(m)/weekday(dow)[last];
              }
              else if (std::isalpha(is.peek()))
              {
                  auto dow = parse_dow(is);
                  char c{};
                  is >> c;
                  if (c == '<' || c == '>')
                  {
                      char c2{};
                      is >> c2;
                      if (c2 != '=')
                          throw std::runtime_error(std::string("bad operator: ") + c + c2);
                      int d;
                      is >> d;
                      if (d < 1 || d > 31)
                          throw std::runtime_error(std::string("bad operator: ") + c + c2
                                   + std::to_string(d));
                      x.type_ = c == '<' ? MonthDayTime::lteq : MonthDayTime::gteq;
                      x.u = MonthDayTime::pair{ date::month(m) / d, date::weekday(dow) };
                  }
                  else
                      throw std::runtime_error(std::string("bad operator: ") + c);
              }
              else  // if (std::isdigit(is.peek())
              {
                  int d;
                  is >> d;
                  if (d < 1 || d > 31)
                      throw std::runtime_error(std::string("day of month: ")
                               + std::to_string(d));
                  x.type_ = MonthDayTime::month_day;
                  x.u = date::month(m)/d;
              }
              if (!is.eof() && ws(is) && !is.eof() && is.peek() != '#')
              {
                  int t;
                  is >> t;
                  x.h_ = hours{t};
                  if (!is.eof() && is.peek() == ':')
                  {
                      is.get();
                      is >> t;
                      x.m_ = minutes{t};
                      if (!is.eof() && is.peek() == ':')
                      {
                          is.get();
                          is >> t;
                          x.s_ = seconds{t};
                      }
                  }
                  if (!is.eof() && std::isalpha(is.peek()))
                  {
                      char c;
                      is >> c;
                      switch (c)
                      {
                      case 's':
                          x.zone_ = tz::standard;
                          break;
                      case 'u':
                          x.zone_ = tz::utc;
                          break;
                      }
                  }
              }
          }
          else
          {
              x.u = month{m}/1;
          }
      }
      return is;
  }
  //}}}
  //{{{
  std::ostream& detail::operator << (std::ostream& os, const MonthDayTime& x)
  {
      switch (x.type_)
      {
      case MonthDayTime::month_day:
          os << x.u.month_day_ << "                  ";
          break;
      case MonthDayTime::month_last_dow:
          os << x.u.month_weekday_last_ << "           ";
          break;
      case MonthDayTime::lteq:
          os << x.u.month_day_weekday_.weekday_ << " on or before "
             << x.u.month_day_weekday_.month_day_ << "  ";
          break;
      case MonthDayTime::gteq:
          if ((static_cast<unsigned>(x.day()) - 1) % 7 == 0)
          {
              os << (x.u.month_day_weekday_.month_day_.month() /
                     x.u.month_day_weekday_.weekday_[
                         (static_cast<unsigned>(x.day()) - 1)/7+1]) << "              ";
          }
          else
          {
              os << x.u.month_day_weekday_.weekday_ << " on or after "
                 << x.u.month_day_weekday_.month_day_ << "  ";
          }
          break;
      }
      os << date::make_time(x.s_ + x.h_ + x.m_);
      if (x.zone_ == tz::utc)
          os << "UTC   ";
      else if (x.zone_ == tz::standard)
          os << "STD   ";
      else
          os << "      ";
      return os;
  }
  //}}}
  //{{{
  // Rule

  detail::Rule::Rule (const std::string& s)
  {
      try
      {
          using namespace date;
          using namespace std::chrono;
          std::istringstream in(s);
          in.exceptions(std::ios::failbit | std::ios::badbit);
          std::string word;
          in >> word >> name_;
          int x;
          ws(in);
          if (std::isalpha(in.peek()))
          {
              in >> word;
              if (word == "min")
              {
                  starting_year_ = year::min();
              }
              else
                  throw std::runtime_error("Didn't find expected word: " + word);
          }
          else
          {
              in >> x;
              starting_year_ = year{x};
          }
          std::ws(in);
          if (std::isalpha(in.peek()))
          {
              in >> word;
              if (word == "only")
              {
                  ending_year_ = starting_year_;
              }
              else if (word == "max")
              {
                  ending_year_ = year::max();
              }
              else
                  throw std::runtime_error("Didn't find expected word: " + word);
          }
          else
          {
              in >> x;
              ending_year_ = year{x};
          }
          in >> word;  // TYPE (always "-")
          assert(word == "-");
          in >> starting_at_;
          save_ = duration_cast<minutes>(parse_signed_time(in));
          in >> abbrev_;
          if (abbrev_ == "-")
              abbrev_.clear();
          assert(hours{0} <= save_ && save_ <= hours{2});
      }
      catch (...)
      {
          std::cerr << s << '\n';
          std::cerr << *this << '\n';
          throw;
      }
  }
  //}}}
  //{{{
  detail::Rule::Rule (const Rule& r, date::year starting_year, date::year ending_year)
      : name_(r.name_)
      , starting_year_(starting_year)
      , ending_year_(ending_year)
      , starting_at_(r.starting_at_)
      , save_(r.save_)
      , abbrev_(r.abbrev_)
  {
  }
  //}}}

  //{{{
  bool detail::operator == (const Rule& x, const Rule& y)
  {
      if (std::tie(x.name_, x.save_, x.starting_year_, x.ending_year_) ==
          std::tie(y.name_, y.save_, y.starting_year_, y.ending_year_))
          return x.month() == y.month() && x.day() == y.day();
      return false;
  }
  //}}}
  //{{{
  bool detail::operator < (const Rule& x, const Rule& y)
  {
      using namespace std::chrono;
      auto const xm = x.month();
      auto const ym = y.month();
      if (std::tie(x.name_, x.starting_year_, xm, x.ending_year_) <
          std::tie(y.name_, y.starting_year_, ym, y.ending_year_))
          return true;
      if (std::tie(x.name_, x.starting_year_, xm, x.ending_year_) >
          std::tie(y.name_, y.starting_year_, ym, y.ending_year_))
          return false;
      return x.day() < y.day();
  }
  //}}}
  //{{{
  bool detail::operator == (const Rule& x, const date::year& y)
  {
      return x.starting_year_ <= y && y <= x.ending_year_;
  }
  //}}}
  //{{{
  bool detail::operator < (const Rule& x, const date::year& y)
  {
      return x.ending_year_ < y;
  }
  //}}}
  //{{{
  bool detail::operator == (const date::year& x, const Rule& y)
  {
      return y.starting_year_ <= x && x <= y.ending_year_;
  }
  //}}}
  //{{{
  bool detail::operator < (const date::year& x, const Rule& y)
  {
      return x < y.starting_year_;
  }
  //}}}
  //{{{
  bool detail::operator == (const Rule& x, const std::string& y)
  {
      return x.name() == y;
  }
  //}}}
  //{{{
  bool detail::operator < (const Rule& x, const std::string& y)
  {
      return x.name() < y;
  }
  //}}}
  //{{{
  bool detail::operator == (const std::string& x, const Rule& y)
  {
      return y.name() == x;
  }
  //}}}
  //{{{
  bool detail::operator < (const std::string& x, const Rule& y)
  {
      return x < y.name();
  }
  //}}}
  //{{{
  std::ostream& detail::operator << (std::ostream& os, const Rule& r)
  {
      using namespace date;
      using namespace std::chrono;
      detail::save_stream<char> _(os);
      os.fill(' ');
      os.flags(std::ios::dec | std::ios::left);
      os.width(15);
      os << r.name_;
      os << r.starting_year_ << "    " << r.ending_year_ << "    ";
      os << r.starting_at_;
      if (r.save_ >= minutes{0})
          os << ' ';
      os << date::make_time(r.save_) << "   ";
      os << r.abbrev_;
      return os;
  }
  //}}}

  //{{{
  date::day detail::Rule::day() const
  {
      return starting_at_.day();
  }
  //}}}
  //{{{
  date::month detail::Rule::month() const
  {
      return starting_at_.month();
  }
  //}}}

  //{{{
  struct find_rule_by_name
  {
      bool operator()(const Rule& x, const std::string& nm) const
      {
          return x.name() < nm;
      }

      bool operator()(const std::string& nm, const Rule& x) const
      {
          return nm < x.name();
      }
  };
  //}}}
  //{{{
  bool detail::Rule::overlaps (const Rule& x, const Rule& y)
  {
      // assume x.starting_year_ <= y.starting_year_;
      if (!(x.starting_year_ <= y.starting_year_))
      {
          std::cerr << x << '\n';
          std::cerr << y << '\n';
          assert(x.starting_year_ <= y.starting_year_);
      }
      if (y.starting_year_ > x.ending_year_)
          return false;
      return !(x.starting_year_ == y.starting_year_ && x.ending_year_ == y.ending_year_);
  }
  //}}}
  //{{{
  void detail::Rule::split (std::vector<Rule>& rules, std::size_t i, std::size_t k, std::size_t& e)
  {
      using namespace date;
      using difference_type = std::vector<Rule>::iterator::difference_type;
      // rules[i].starting_year_ <= rules[k].starting_year_ &&
      //     rules[i].ending_year_ >= rules[k].starting_year_ &&
      //     (rules[i].starting_year_ != rules[k].starting_year_ ||
      //      rules[i].ending_year_ != rules[k].ending_year_)
      assert(rules[i].starting_year_ <= rules[k].starting_year_ &&
             rules[i].ending_year_ >= rules[k].starting_year_ &&
             (rules[i].starting_year_ != rules[k].starting_year_ ||
              rules[i].ending_year_ != rules[k].ending_year_));
      if (rules[i].starting_year_ == rules[k].starting_year_)
      {
          if (rules[k].ending_year_ < rules[i].ending_year_)
          {
              rules.insert(rules.begin() + static_cast<difference_type>(k+1),
                           Rule(rules[i], rules[k].ending_year_ + years{1},
                                std::move(rules[i].ending_year_)));
              ++e;
              rules[i].ending_year_ = rules[k].ending_year_;
          }
          else  // rules[k].ending_year_ > rules[i].ending_year_
          {
              rules.insert(rules.begin() + static_cast<difference_type>(k+1),
                           Rule(rules[k], rules[i].ending_year_ + years{1},
                                std::move(rules[k].ending_year_)));
              ++e;
              rules[k].ending_year_ = rules[i].ending_year_;
          }
      }
      else  // rules[i].starting_year_ < rules[k].starting_year_
      {
          if (rules[k].ending_year_ < rules[i].ending_year_)
          {
              rules.insert(rules.begin() + static_cast<difference_type>(k),
                           Rule(rules[i], rules[k].starting_year_, rules[k].ending_year_));
              ++k;
              rules.insert(rules.begin() + static_cast<difference_type>(k+1),
                           Rule(rules[i], rules[k].ending_year_ + years{1},
                                std::move(rules[i].ending_year_)));
              rules[i].ending_year_ = rules[k].starting_year_ - years{1};
              e += 2;
          }
          else if (rules[k].ending_year_ > rules[i].ending_year_)
          {
              rules.insert(rules.begin() + static_cast<difference_type>(k),
                           Rule(rules[i], rules[k].starting_year_, rules[i].ending_year_));
              ++k;
              rules.insert(rules.begin() + static_cast<difference_type>(k+1),
                           Rule(rules[k], rules[i].ending_year_ + years{1},
                           std::move(rules[k].ending_year_)));
              e += 2;
              rules[k].ending_year_ = std::move(rules[i].ending_year_);
              rules[i].ending_year_ = rules[k].starting_year_ - years{1};
          }
          else  // rules[k].ending_year_ == rules[i].ending_year_
          {
              rules.insert(rules.begin() + static_cast<difference_type>(k),
                           Rule(rules[i], rules[k].starting_year_,
                           std::move(rules[i].ending_year_)));
              ++k;
              ++e;
              rules[i].ending_year_ = rules[k].starting_year_ - years{1};
          }
      }
  }
  //}}}
  //{{{
  void detail::Rule::split_overlaps (std::vector<Rule>& rules, std::size_t i, std::size_t& e)
  {
      using difference_type = std::vector<Rule>::iterator::difference_type;
      auto j = i;
      for (; i + 1 < e; ++i)
      {
          for (auto k = i + 1; k < e; ++k)
          {
              if (overlaps(rules[i], rules[k]))
              {
                  split(rules, i, k, e);
                  std::sort(rules.begin() + static_cast<difference_type>(i),
                            rules.begin() + static_cast<difference_type>(e));
              }
          }
      }
      for (; j < e; ++j)
      {
          if (rules[j].starting_year() == rules[j].ending_year())
              rules[j].starting_at_.canonicalize(rules[j].starting_year());
      }
  }
  //}}}
  //{{{
  void detail::Rule::split_overlaps (std::vector<Rule>& rules)
  {
      using difference_type = std::vector<Rule>::iterator::difference_type;
      for (std::size_t i = 0; i < rules.size();)
      {
          auto e = static_cast<std::size_t>(std::upper_bound(
              rules.cbegin()+static_cast<difference_type>(i), rules.cend(), rules[i].name(),
              [](const std::string& nm, const Rule& x)
              {
                  return nm < x.name();
              }) - rules.cbegin());
          split_overlaps(rules, i, e);
          auto first_rule = rules.begin() + static_cast<difference_type>(i);
          auto last_rule = rules.begin() + static_cast<difference_type>(e);
          auto t = std::lower_bound(first_rule, last_rule, min_year);
          if (t > first_rule+1)
          {
              if (t == last_rule || t->starting_year() >= min_year)
                  --t;
              auto d = static_cast<std::size_t>(t - first_rule);
              rules.erase(first_rule, t);
              e -= d;
          }
          first_rule = rules.begin() + static_cast<difference_type>(i);
          last_rule = rules.begin() + static_cast<difference_type>(e);
          t = std::upper_bound(first_rule, last_rule, max_year);
          if (t != last_rule)
          {
              auto d = static_cast<std::size_t>(last_rule - t);
              rules.erase(t, last_rule);
              e -= d;
          }
          i = e;
      }
      rules.shrink_to_fit();
  }
  //}}}

  //{{{
  // Find the rule that comes chronologically before Rule r.  For multi-year rules,
  // y specifies which rules in r.  For single year rules, y is assumed to be equal
  // to the year specified by r.
  // Returns a pointer to the chronologically previous rule, and the year within
  // that rule.  If there is no previous rule, returns nullptr and year::min().
  // Preconditions:
  //     r->starting_year() <= y && y <= r->ending_year()
  static std::pair<const Rule*, date::year> find_previous_rule (const Rule* r, date::year y)
  {
      using namespace date;
      auto const& rules = get_tzdb().rules;
      if (y == r->starting_year())
      {
          if (r == &rules.front() || r->name() != r[-1].name())
              std::terminate();  // never called with first rule
          --r;
          if (y == r->starting_year())
              return {r, y};
          return {r, r->ending_year()};
      }
      if (r == &rules.front() || r->name() != r[-1].name() ||
          r[-1].starting_year() < r->starting_year())
      {
          while (r < &rules.back() && r->name() == r[1].name() &&
                 r->starting_year() == r[1].starting_year())
              ++r;
          return {r, --y};
      }
      --r;
      return {r, y};
  }
  //}}}
  //{{{
  // Find the rule that comes chronologically after Rule r.  For multi-year rules,
  // y specifies which rules in r.  For single year rules, y is assumed to be equal
  // to the year specified by r.
  // Returns a pointer to the chronologically next rule, and the year within
  // that rule.  If there is no next rule, return a pointer to a defaulted rule
  // and y+1.
  // Preconditions:
  //     first <= r && r < last && r->starting_year() <= y && y <= r->ending_year()
  //     [first, last) all have the same name
  static std::pair<const Rule*, date::year> find_next_rule (const Rule* first_rule, const Rule* last_rule, const Rule* r, date::year y)
  {
      using namespace date;
      if (y == r->ending_year())
      {
          if (r == last_rule-1)
              return {nullptr, year::max()};
          ++r;
          if (y == r->ending_year())
              return {r, y};
          return {r, r->starting_year()};
      }
      if (r == last_rule-1 || r->ending_year() < r[1].ending_year())
      {
          while (r > first_rule && r->starting_year() == r[-1].starting_year())
              --r;
          return {r, ++y};
      }
      ++r;
      return {r, y};
  }
  //}}}
  //{{{
  // Find the rule that comes chronologically after Rule r.  For multi-year rules,
  // y specifies which rules in r.  For single year rules, y is assumed to be equal
  // to the year specified by r.
  // Returns a pointer to the chronologically next rule, and the year within
  // that rule.  If there is no next rule, return nullptr and year::max().
  // Preconditions:
  //     r->starting_year() <= y && y <= r->ending_year()
  static std::pair<const Rule*, date::year> find_next_rule (const Rule* r, date::year y)
  {
      using namespace date;
      auto const& rules = get_tzdb().rules;
      if (y == r->ending_year())
      {
          if (r == &rules.back() || r->name() != r[1].name())
              return {nullptr, year::max()};
          ++r;
          if (y == r->ending_year())
              return {r, y};
          return {r, r->starting_year()};
      }
      if (r == &rules.back() || r->name() != r[1].name() ||
          r->ending_year() < r[1].ending_year())
      {
          while (r > &rules.front() && r->name() == r[-1].name() &&
                 r->starting_year() == r[-1].starting_year())
              --r;
          return {r, ++y};
      }
      ++r;
      return {r, y};
  }
  //}}}
  //{{{
  static const Rule* find_first_std_rule (const std::pair<const Rule*, const Rule*>& eqr)
  {
      auto r = eqr.first;
      auto ry = r->starting_year();
      while (r->save() != std::chrono::minutes{0})
      {
          std::tie(r, ry) = find_next_rule(eqr.first, eqr.second, r, ry);
          if (r == nullptr)
              throw std::runtime_error("Could not find standard offset in rule "
                                       + eqr.first->name());
      }
      return r;
  }
  //}}}
  //{{{
  static std::pair<const Rule*, date::year> find_rule_for_zone (const std::pair<const Rule*, const Rule*>& eqr,
                     const date::year& y, const std::chrono::seconds& offset,
                     const MonthDayTime& mdt)
  {
      assert(eqr.first != nullptr);
      assert(eqr.second != nullptr);

      using namespace std::chrono;
      using namespace date;
      auto r = eqr.first;
      auto ry = r->starting_year();
      auto prev_save = minutes{0};
      auto prev_year = year::min();
      const Rule* prev_rule = nullptr;
      while (r != nullptr)
      {
          if (mdt.compare(y, r->mdt(), ry, offset, prev_save) <= 0)
              break;
          prev_rule = r;
          prev_year = ry;
          prev_save = prev_rule->save();
          std::tie(r, ry) = find_next_rule(eqr.first, eqr.second, r, ry);
      }
      return {prev_rule, prev_year};
  }
  //}}}
  //{{{
  static std::pair<const Rule*, date::year> find_rule_for_zone (const std::pair<const Rule*, const Rule*>& eqr,
                     const sys_seconds& tp_utc,
                     const local_seconds& tp_std,
                     const local_seconds& tp_loc)
  {
      using namespace std::chrono;
      using namespace date;
      auto r = eqr.first;
      auto ry = r->starting_year();
      auto prev_save = minutes{0};
      auto prev_year = year::min();
      const Rule* prev_rule = nullptr;
      while (r != nullptr)
      {
          bool found = false;
          switch (r->mdt().zone())
          {
          case tz::utc:
              found = tp_utc < r->mdt().to_time_point(ry);
              break;
          case tz::standard:
              found = sys_seconds{tp_std.time_since_epoch()} < r->mdt().to_time_point(ry);
              break;
          case tz::local:
              found = sys_seconds{tp_loc.time_since_epoch()} < r->mdt().to_time_point(ry);
              break;
          }
          if (found)
              break;
          prev_rule = r;
          prev_year = ry;
          prev_save = prev_rule->save();
          std::tie(r, ry) = find_next_rule(eqr.first, eqr.second, r, ry);
      }
      return {prev_rule, prev_year};
  }
  //}}}
  //{{{
  static sys_info find_rule (const std::pair<const Rule*, date::year>& first_rule,
            const std::pair<const Rule*, date::year>& last_rule,
            const date::year& y, const std::chrono::seconds& offset,
            const MonthDayTime& mdt, const std::chrono::minutes& initial_save,
            const std::string& initial_abbrev)
  {
      using namespace std::chrono;
      using namespace date;
      auto r = first_rule.first;
      auto ry = first_rule.second;
      sys_info x{sys_days(year::min()/min_day), sys_days(year::max()/max_day),
                 seconds{0}, initial_save, initial_abbrev};
      while (r != nullptr)
      {
          auto tr = r->mdt().to_sys(ry, offset, x.save);
          auto tx = mdt.to_sys(y, offset, x.save);
          // Find last rule where tx >= tr
          if (tx <= tr || (r == last_rule.first && ry == last_rule.second))
          {
              if (tx < tr && r == first_rule.first && ry == first_rule.second)
              {
                  x.end = r->mdt().to_sys(ry, offset, x.save);
                  break;
              }
              if (tx < tr)
              {
                  std::tie(r, ry) = find_previous_rule(r, ry);  // can't return nullptr for r
                  assert(r != nullptr);
              }
              // r != nullptr && tx >= tr (if tr were to be recomputed)
              auto prev_save = initial_save;
              if (!(r == first_rule.first && ry == first_rule.second))
                  prev_save = find_previous_rule(r, ry).first->save();
              x.begin = r->mdt().to_sys(ry, offset, prev_save);
              x.save = r->save();
              x.abbrev = r->abbrev();
              if (!(r == last_rule.first && ry == last_rule.second))
              {
                  std::tie(r, ry) = find_next_rule(r, ry);  // can't return nullptr for r
                  assert(r != nullptr);
                  x.end = r->mdt().to_sys(ry, offset, x.save);
              }
              else
                  x.end = sys_days(year::max()/max_day);
              break;
          }
          x.save = r->save();
          std::tie(r, ry) = find_next_rule(r, ry);  // Can't return nullptr for r
          assert(r != nullptr);
      }
      return x;
  }
  //}}}

  // zonelet
  //{{{
  detail::zonelet::~zonelet() {
      using minutes = std::chrono::minutes;
      using string = std::string;
      if (tag_ == has_save)
          u.save_.~minutes();
      else
          u.rule_.~string();
  }
  //}}}
  //{{{
  detail::zonelet::zonelet() {
      ::new(&u.rule_) std::string();
  }
  //}}}
  //{{{
  detail::zonelet::zonelet (const zonelet& i)
      : gmtoff_(i.gmtoff_)
      , tag_(i.tag_)
      , format_(i.format_)
      , until_year_(i.until_year_)
      , until_date_(i.until_date_)
      , until_utc_(i.until_utc_)
      , until_std_(i.until_std_)
      , until_loc_(i.until_loc_)
      , initial_save_(i.initial_save_)
      , initial_abbrev_(i.initial_abbrev_)
      , first_rule_(i.first_rule_)
      , last_rule_(i.last_rule_)
  {
      if (tag_ == has_save)
          ::new(&u.save_) std::chrono::minutes(i.u.save_);
      else
          ::new(&u.rule_) std::string(i.u.rule_);
  }
  //}}}

  // time_zone
  //{{{
  time_zone::time_zone (const std::string& s, detail::undocumented)
      : adjusted_(new std::once_flag{})
  {
      try
      {
          using namespace date;
          std::istringstream in(s);
          in.exceptions(std::ios::failbit | std::ios::badbit);
          std::string word;
          in >> word >> name_;
          parse_info(in);
      }
      catch (...)
      {
          std::cerr << s << '\n';
          std::cerr << *this << '\n';
          zonelets_.pop_back();
          throw;
      }
  }
  //}}}
  //{{{
  sys_info time_zone::get_info_impl (sys_seconds tp) const
  {
      return get_info_impl(tp, static_cast<int>(tz::utc));
  }
  //}}}
  //{{{
  local_info time_zone::get_info_impl (local_seconds tp) const
  {
      using namespace std::chrono;
      local_info i{};
      i.first = get_info_impl(sys_seconds{tp.time_since_epoch()}, static_cast<int>(tz::local));
      auto tps = sys_seconds{(tp - i.first.offset).time_since_epoch()};
      if (tps < i.first.begin)
      {
          i.second = std::move(i.first);
          i.first = get_info_impl(i.second.begin - seconds{1}, static_cast<int>(tz::utc));
          i.result = local_info::nonexistent;
      }
      else if (i.first.end - tps <= days{1})
      {
          i.second = get_info_impl(i.first.end, static_cast<int>(tz::utc));
          tps = sys_seconds{(tp - i.second.offset).time_since_epoch()};
          if (tps >= i.second.begin)
              i.result = local_info::ambiguous;
          else
              i.second = {};
      }
      return i;
  }
  //}}}

  //{{{
  void time_zone::add (const std::string& s)
  {
      try
      {
          std::istringstream in(s);
          in.exceptions(std::ios::failbit | std::ios::badbit);
          ws(in);
          if (!in.eof() && in.peek() != '#')
              parse_info(in);
      }
      catch (...)
      {
          std::cerr << s << '\n';
          std::cerr << *this << '\n';
          zonelets_.pop_back();
          throw;
      }
  }
  //}}}
  //{{{
  void time_zone::parse_info (std::istream& in)
  {
      using namespace date;
      using namespace std::chrono;
      zonelets_.emplace_back();
      auto& zonelet = zonelets_.back();
      zonelet.gmtoff_ = parse_signed_time(in);
      in >> zonelet.u.rule_;
      if (zonelet.u.rule_ == "-")
          zonelet.u.rule_.clear();
      in >> zonelet.format_;
      if (!in.eof())
          ws(in);
      if (in.eof() || in.peek() == '#')
      {
          zonelet.until_year_ = year::max();
          zonelet.until_date_ = MonthDayTime(max_day, tz::utc);
      }
      else
      {
          int y;
          in >> y;
          zonelet.until_year_ = year{y};
          in >> zonelet.until_date_;
          zonelet.until_date_.canonicalize(zonelet.until_year_);
      }
      if ((zonelet.until_year_ < min_year) ||
              (zonelets_.size() > 1 && zonelets_.end()[-2].until_year_ > max_year))
          zonelets_.pop_back();
  }
  //}}}
  //{{{
  void time_zone::adjust_infos (const std::vector<Rule>& rules)
  {
      using namespace std::chrono;
      using namespace date;
      const zonelet* prev_zonelet = nullptr;
      for (auto& z : zonelets_)
      {
          std::pair<const Rule*, const Rule*> eqr{};
          std::istringstream in;
          in.exceptions(std::ios::failbit | std::ios::badbit);
          // Classify info as rule-based, has save, or neither
          if (!z.u.rule_.empty())
          {
              // Find out if this zonelet has a rule or a save
              eqr = std::equal_range(rules.data(), rules.data() + rules.size(), z.u.rule_);
              if (eqr.first == eqr.second)
              {
                  // The rule doesn't exist.  Assume this is a save
                  try
                  {
                      using namespace std::chrono;
                      using string = std::string;
                      in.str(z.u.rule_);
                      auto tmp = duration_cast<minutes>(parse_signed_time(in));
                      z.u.rule_.~string();
                      z.tag_ = zonelet::has_save;
                      ::new(&z.u.save_) minutes(tmp);
                  }
                  catch (...)
                  {
                      std::cerr << name_ << " : " << z.u.rule_ << '\n';
                      throw;
                  }
              }
          }
          else
          {
              // This zone::zonelet has no rule and no save
              z.tag_ = zonelet::is_empty;
          }

          minutes final_save{0};
          if (z.tag_ == zonelet::has_save)
          {
              final_save = z.u.save_;
          }
          else if (z.tag_ == zonelet::has_rule)
          {
              z.last_rule_ = find_rule_for_zone(eqr, z.until_year_, z.gmtoff_,
                                                z.until_date_);
              if (z.last_rule_.first != nullptr)
                  final_save = z.last_rule_.first->save();
          }
          z.until_utc_ = z.until_date_.to_sys(z.until_year_, z.gmtoff_, final_save);
          z.until_std_ = local_seconds{z.until_utc_.time_since_epoch()} + z.gmtoff_;
          z.until_loc_ = z.until_std_ + final_save;

          if (z.tag_ == zonelet::has_rule)
          {
              if (prev_zonelet != nullptr)
              {
                  z.first_rule_ = find_rule_for_zone(eqr, prev_zonelet->until_utc_,
                                                          prev_zonelet->until_std_,
                                                          prev_zonelet->until_loc_);
                  if (z.first_rule_.first != nullptr)
                  {
                      z.initial_save_ = z.first_rule_.first->save();
                      z.initial_abbrev_ = z.first_rule_.first->abbrev();
                      if (z.first_rule_ != z.last_rule_)
                      {
                          z.first_rule_ = find_next_rule(eqr.first, eqr.second,
                                                         z.first_rule_.first,
                                                         z.first_rule_.second);
                      }
                      else
                      {
                          z.first_rule_ = std::make_pair(nullptr, year::min());
                          z.last_rule_ = std::make_pair(nullptr, year::max());
                      }
                  }
              }
              if (z.first_rule_.first == nullptr && z.last_rule_.first != nullptr)
              {
                  z.first_rule_ = std::make_pair(eqr.first, eqr.first->starting_year());
                  z.initial_abbrev_ = find_first_std_rule(eqr)->abbrev();
              }
          }

  #ifndef NDEBUG
          if (z.first_rule_.first == nullptr)
          {
              assert(z.first_rule_.second == year::min());
              assert(z.last_rule_.first == nullptr);
              assert(z.last_rule_.second == year::max());
          }
          else
          {
              assert(z.last_rule_.first != nullptr);
          }
  #endif
          prev_zonelet = &z;
      }
  }
  //}}}
  //{{{
  static std::string format_abbrev (std::string format, const std::string& variable, std::chrono::seconds off,
                                                                 std::chrono::minutes save)
  {
      using namespace std::chrono;
      auto k = format.find("%s");
      if (k != std::string::npos)
      {
          format.replace(k, 2, variable);
      }
      else
      {
          k = format.find('/');
          if (k != std::string::npos)
          {
              if (save == minutes{0})
                  format.erase(k);
              else
                  format.erase(0, k+1);
          }
          else
          {
              k = format.find("%z");
              if (k != std::string::npos)
              {
                  std::string temp;
                  if (off < seconds{0})
                  {
                      temp = '-';
                      off = -off;
                  }
                  else
                      temp = '+';
                  auto h = date::floor<hours>(off);
                  off -= h;
                  if (h < hours{10})
                      temp += '0';
                  temp += std::to_string(h.count());
                  if (off > seconds{0})
                  {
                      auto m = date::floor<minutes>(off);
                      off -= m;
                      if (m < minutes{10})
                          temp += '0';
                      temp += std::to_string(m.count());
                      if (off > seconds{0})
                      {
                          if (off < seconds{10})
                              temp += '0';
                          temp += std::to_string(off.count());
                      }
                  }
                  format.replace(k, 2, temp);
              }
          }
      }
      return format;
  }
  //}}}
  //{{{
  sys_info time_zone::get_info_impl (sys_seconds tp, int tz_int) const
  {
      using namespace std::chrono;
      using namespace date;
      tz timezone = static_cast<tz>(tz_int);
      assert(timezone != tz::standard);
      auto y = year_month_day(floor<days>(tp)).year();
      if (y < min_year || y > max_year)
          throw std::runtime_error("The year " + std::to_string(static_cast<int>(y)) +
              " is out of range:[" + std::to_string(static_cast<int>(min_year)) + ", "
                                   + std::to_string(static_cast<int>(max_year)) + "]");
      std::call_once(*adjusted_,
                     [this]()
                     {
                         const_cast<time_zone*>(this)->adjust_infos(get_tzdb().rules);
                     });
      auto i = std::upper_bound(zonelets_.begin(), zonelets_.end(), tp,
          [timezone](sys_seconds t, const zonelet& zl)
          {
              return timezone == tz::utc ? t < zl.until_utc_ :
                                           t < sys_seconds{zl.until_loc_.time_since_epoch()};
          });

      sys_info r{};
      if (i != zonelets_.end())
      {
          if (i->tag_ == zonelet::has_save)
          {
              if (i != zonelets_.begin())
                  r.begin = i[-1].until_utc_;
              else
                  r.begin = sys_days(year::min()/min_day);
              r.end = i->until_utc_;
              r.offset = i->gmtoff_ + i->u.save_;
              r.save = i->u.save_;
          }
          else if (i->u.rule_.empty())
          {
              if (i != zonelets_.begin())
                  r.begin = i[-1].until_utc_;
              else
                  r.begin = sys_days(year::min()/min_day);
              r.end = i->until_utc_;
              r.offset = i->gmtoff_;
          }
          else
          {
              r = find_rule(i->first_rule_, i->last_rule_, y, i->gmtoff_,
                            MonthDayTime(local_seconds{tp.time_since_epoch()}, timezone),
                            i->initial_save_, i->initial_abbrev_);
              r.offset = i->gmtoff_ + r.save;
              if (i != zonelets_.begin() && r.begin < i[-1].until_utc_)
                  r.begin = i[-1].until_utc_;
              if (r.end > i->until_utc_)
                  r.end = i->until_utc_;
          }
          r.abbrev = format_abbrev(i->format_, r.abbrev, r.offset, r.save);
          assert(r.begin < r.end);
      }
      return r;
  }
  //}}}
  //{{{
  std::ostream& operator << (std::ostream& os, const time_zone& z)
  {
      using namespace date;
      using namespace std::chrono;
      detail::save_stream<char> _(os);
      os.fill(' ');
      os.flags(std::ios::dec | std::ios::left);
      std::call_once(*z.adjusted_,
                     [&z]()
                     {
                         const_cast<time_zone&>(z).adjust_infos(get_tzdb().rules);
                     });
      os.width(35);
      os << z.name_;
      std::string indent;
      for (auto const& s : z.zonelets_)
      {
          os << indent;
          if (s.gmtoff_ >= seconds{0})
              os << ' ';
          os << make_time(s.gmtoff_) << "   ";
          os.width(15);
          if (s.tag_ != zonelet::has_save)
              os << s.u.rule_;
          else
          {
              std::ostringstream tmp;
              tmp << make_time(s.u.save_);
              os <<  tmp.str();
          }
          os.width(8);
          os << s.format_ << "   ";
          os << s.until_year_ << ' ' << s.until_date_;
          os << "   " << s.until_utc_ << " UTC";
          os << "   " << s.until_std_ << " STD";
          os << "   " << s.until_loc_;
          os << "   " << make_time(s.initial_save_);
          os << "   " << s.initial_abbrev_;
          if (s.first_rule_.first != nullptr)
              os << "   {" << *s.first_rule_.first << ", " << s.first_rule_.second << '}';
          else
              os << "   {" << "nullptr" << ", " << s.first_rule_.second << '}';
          if (s.last_rule_.first != nullptr)
              os << "   {" << *s.last_rule_.first << ", " << s.last_rule_.second << '}';
          else
              os << "   {" << "nullptr" << ", " << s.last_rule_.second << '}';
          os << '\n';
          if (indent.empty())
              indent = std::string(35, ' ');
      }
      return os;
  }
  //}}}

  #if !MISSING_LEAP_SECONDS
  //{{{
  std::ostream& operator << (std::ostream& os, const leap& x)
  {
      using namespace date;
      return os << x.date_ << "  +";
  }
  //}}}
  #endif  // !MISSING_LEAP_SECONDS
  }
