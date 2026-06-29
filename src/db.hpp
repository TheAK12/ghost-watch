#pragma once
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

struct AppStat {
  std::string name;
  int duration = 0;
  int sessions = 0;
};
struct TitleStat {
  std::string app;
  std::string title;
  int duration = 0;
};
struct DayStat {
  std::string date;
  std::string full;
  int total = 0;
};
struct Goal {
  std::string app;
  int limit_seconds = 0;
  bool notified = false;
};
struct PomodoroState {
  bool active = false;
  bool visible = false;
  bool on_break = false;
  int work_seconds = 25 * 60;
  int break_seconds = 5 * 60;
  int elapsed = 0;
  int sessions_done = 0;
};
struct DBData {
  int total_today = 0;
  int total_yesterday = 0;
  int peak_hour = -1;
  int peak_hour_val = 0;
  std::map<int, int> hourly;
  std::vector<AppStat> apps;
  std::vector<TitleStat> titles;
  std::vector<DayStat> history;
  std::string active_app = "None";
  bool is_idle = false;
  std::map<std::string, int> streaks;
  int history_days = 30;
};

inline bool is_system_idle() {
  struct stat st;
  return stat("/tmp/ghost-watch-idle", &st) == 0;
}

inline std::string db_path() { return "screentime.db"; }

class DB {
public:
  explicit DB(const std::string &path) { sqlite3_open(path.c_str(), &db_); }
  ~DB() {
    if (db_)
      sqlite3_close(db_);
  }
  void query(const std::string &sql, std::function<void(sqlite3_stmt *)> cb,
             std::function<void(sqlite3_stmt *)> bind = {}) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
      return;
    if (bind)
      bind(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW)
      cb(stmt);
    sqlite3_finalize(stmt);
  }
  static std::string str(sqlite3_stmt *s, int col) {
    auto p = reinterpret_cast<const char *>(sqlite3_column_text(s, col));
    return p ? p : "";
  }
  static int integer(sqlite3_stmt *s, int col) {
    return sqlite3_column_int(s, col);
  }

private:
  sqlite3 *db_ = nullptr;
};

inline DBData fetch_db(int history_days = 30) {
  DBData d;
  d.history_days = history_days;
  d.is_idle = is_system_idle();
  for (int i = 0; i < 24; ++i)
    d.hourly[i] = 0;
  DB db(db_path());

  db.query("SELECT cast(strftime('%H',timestamp,'localtime') as integer), "
           "SUM(duration_seconds) FROM window_usage WHERE "
           "date(timestamp,'localtime')=date('now','localtime') GROUP BY 1;",
           [&](sqlite3_stmt *s) {
             int h = DB::integer(s, 0);
             int v = DB::integer(s, 1);
             if (h >= 0 && h <= 23) {
               d.hourly[h] = v;
               d.total_today += v;
             }
           });
  db.query("SELECT COALESCE(SUM(duration_seconds),0) FROM window_usage WHERE "
           "date(timestamp,'localtime')=date('now','-1 day','localtime');",
           [&](sqlite3_stmt *s) { d.total_yesterday = DB::integer(s, 0); });
  for (auto &[h, v] : d.hourly)
    if (v > d.peak_hour_val) {
      d.peak_hour_val = v;
      d.peak_hour = h;
    }

  db.query("SELECT app_id, SUM(duration_seconds), COUNT(*) FROM window_usage "
           "WHERE date(timestamp,'localtime')=date('now','localtime') GROUP BY "
           "app_id ORDER BY 2 DESC;",
           [&](sqlite3_stmt *s) {
             d.apps.push_back(
                 {DB::str(s, 0), DB::integer(s, 1), DB::integer(s, 2)});
           });
  db.query(
      "SELECT app_id, window_title, SUM(duration_seconds) FROM window_usage "
      "WHERE date(timestamp,'localtime')=date('now','localtime') GROUP BY "
      "app_id, window_title ORDER BY 3 DESC LIMIT 200;",
      [&](sqlite3_stmt *s) {
        d.titles.push_back({DB::str(s, 0), DB::str(s, 1), DB::integer(s, 2)});
      });
  db.query("SELECT app_id FROM window_usage ORDER BY timestamp DESC LIMIT 1;",
           [&](sqlite3_stmt *s) { d.active_app = DB::str(s, 0); });

  for (int i = history_days - 1; i >= 0; --i) {
    char buf[32];
    snprintf(buf, sizeof(buf), "-%d days", i);
    std::string offset(buf);
    db.query("SELECT date('now','" + offset +
                 "','localtime'), COALESCE((SELECT SUM(duration_seconds) FROM "
                 "window_usage WHERE date(timestamp,'localtime')=date('now','" +
                 offset + "','localtime')),0);",
             [&](sqlite3_stmt *s) {
               std::string full = DB::str(s, 0);
               std::string label = full.size() >= 10 ? full.substr(5) : full;
               d.history.push_back({label, full, DB::integer(s, 1)});
             });
  }
  for (auto &app_stat : d.apps) {
    int streak = 0;
    for (int i = 0; i < 90; ++i) {
      char buf[32];
      snprintf(buf, sizeof(buf), "-%d days", i);
      std::string offset(buf);
      bool used = false;
      db.query(
          "SELECT 1 FROM window_usage WHERE app_id=? AND "
          "date(timestamp,'localtime')=date('now','" +
              offset + "','localtime') LIMIT 1;",
          [&](sqlite3_stmt *) { used = true; },
          [&](sqlite3_stmt *s) {
            sqlite3_bind_text(s, 1, app_stat.name.c_str(), -1,
                              SQLITE_TRANSIENT);
          });
      if (used)
        ++streak;
      else
        break;
    }
    d.streaks[app_stat.name] = streak;
  }
  return d;
}

inline std::string goals_path() {
  const char *home = std::getenv("HOME");
  return home ? std::string(home) + "/.config/ghost-watch/goals.conf"
              : "goals.conf";
}
inline std::vector<Goal> load_goals() {
  std::vector<Goal> goals;
  std::ifstream f(goals_path());
  if (!f)
    return goals;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream ss(line);
    Goal g;
    std::string limit_str;
    std::getline(ss, g.app, '|');
    std::getline(ss, limit_str);
    g.limit_seconds = std::stoi(limit_str);
    goals.push_back(g);
  }
  return goals;
}
inline void save_goals(const std::vector<Goal> &goals) {
  std::string p = goals_path();
  std::string dir = p.substr(0, p.rfind('/'));
  std::system(("mkdir -p " + dir).c_str());
  std::ofstream f(p);
  for (auto &g : goals)
    f << g.app << "|" << g.limit_seconds << "\n";
}
