/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "fnord/io/filerepository.h"
#include "fnord/io/fileutil.h"
#include "fnord/application.h"
#include "fnord/logging.h"
#include "fnord/random.h"
#include "fnord/thread/eventloop.h"
#include "fnord/thread/threadpool.h"
#include "fnord/wallclock.h"
#include "fnord/VFS.h"
#include "fnord/rpc/ServerGroup.h"
#include "fnord/rpc/RPC.h"
#include "fnord/rpc/RPCClient.h"
#include "fnord/cli/flagparser.h"
#include "fnord/json/json.h"
#include "fnord/json/jsonrpc.h"
#include "fnord/http/httprouter.h"
#include "fnord/http/httpserver.h"
#include "fnord/http/VFSFileServlet.h"
#include "brokerd/FeedService.h"
#include "brokerd/RemoteFeedFactory.h"
#include "brokerd/RemoteFeedReader.h"
#include "fnord/stats/statsdagent.h"
#include "sstable/SSTableServlet.h"
#include "fnord/mdb/MDB.h"
#include "fnord/mdb/MDBUtil.h"
#include "common.h"
#include "CustomerNamespace.h"
#include "ModelCache.h"
#include "AutoCompleteServlet.h"
#include "AutoCompleteModel.h"
#include "analytics/TermInfoTableSource.h"

using namespace cm;
using namespace fnord;

int main(int argc, const char** argv) {
  fnord::Application::init();
  fnord::Application::logToStderr();

  fnord::cli::FlagParser flags;

  flags.defineFlag(
      "conf",
      cli::FlagParser::T_STRING,
      false,
      NULL,
      "./conf",
      "conf directory",
      "<path>");

  flags.defineFlag(
      "http_port",
      fnord::cli::FlagParser::T_INTEGER,
      false,
      NULL,
      "8000",
      "Start the public http server on this port",
      "<port>");

  flags.defineFlag(
      "datadir",
      cli::FlagParser::T_STRING,
      true,
      NULL,
      NULL,
      "datadir",
      "<path>");

  flags.defineFlag(
      "statsd",
      cli::FlagParser::T_STRING,
      false,
      NULL,
      "127.0.0.1:8192",
      "Statsd addr",
      "<addr>");

  flags.defineFlag(
      "loglevel",
      fnord::cli::FlagParser::T_STRING,
      false,
      NULL,
      "INFO",
      "loglevel",
      "<level>");

  flags.parseArgv(argc, argv);

  Logger::get()->setMinimumLogLevel(
      strToLogLevel(flags.getString("loglevel")));

  auto conf_path = flags.getString("conf");
  auto analyzer = RefPtr<fts::Analyzer>(new fts::Analyzer(conf_path));

  ModelCache models(flags.getString("datadir"));
  models.addModelFactory(
      "AutoCompleteModel", 
      [analyzer] (const String& filepath) -> RefCounted* {
        return new AutoCompleteModel(filepath, analyzer);
      });

  // preheat
  models.getModel("AutoCompleteModel", "termstats", "termstats-dawanda");

  cm::AutoCompleteServlet acservlet(&models);

  /* start http server */
  fnord::thread::EventLoop ev;
  fnord::thread::ThreadPool tpool;
  fnord::http::HTTPRouter http_router;
  http_router.addRouteByPrefixMatch("/autocomplete", &acservlet, &tpool);
  fnord::http::HTTPServer http_server(&http_router, &ev);
  http_server.listen(flags.getInt("http_port"));

  /* stats reporting */
  stats::StatsdAgent statsd_agent(
      InetAddr::resolve(flags.getString("statsd")),
      10 * kMicrosPerSecond);

  statsd_agent.start();

  ev.run();
  return 0;
}
