#include <iostream>
#include <sstream>
#include <string>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <inttypes.h>
#include <boost/circular_buffer.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "sylar/http/http_server.h"
#include "sylar/noncopyable.h"
#include "sylar/timestamp.h"
#include "sylar/stringpiece.h"
#include "sylar/file_utils.h"
#include "sylar/process_info.h"

#include "plot.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

// represent parsed /proc/pid/stat
struct StatData {
    void parse(const char* startAtState, int kbPerPage) {
        // size_t pos = stat.find(')');
        // if (pos == stat.npos) {
            // return;
        // }
        // pos += 2;  // start with state
        // std::string startAtState(stat.c_str() + pos, stat.size() - pos);
        std::istringstream iss(startAtState);

        /// cat /proc/3327/stat
        /*
        3327 (dummyload) S 3161 3327 3161 34821 3327 4194304 439 0 0 0 573 10 0 0 20 0 2 0 
        135290 24911872 2656 18446744073709551615 99751965802496 99751966120605 140734282869680 
        0 0 0 0 0 0 0 0 0 17 2 0 0 0 0 0 99751966308664 99751966319424 99751972614144 
        140734282876623 140734282876642 140734282876642 140734282878956 0
        */

        iss >> state;
        iss >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags;
        iss >> minflt >> cminflt >> majflt >> cmajflt;
        iss >> utime >> stime >> cutime >> cstime;
        iss >> priority >> nice >> num_threads >> itrealvalue >> starttime;
        /// 进程的虚拟内存和驻留集大小
        long vsize, rss;    
        iss >> vsize >> rss >> rsslim;
        vsizeKb = vsize / 1024;
        rssKb = rss * kbPerPage;
    }

    // int pid
    char state;         /// 进程状态
    int ppid;           /// 进程 id
    int pgrp;           /// 进程组 id
    int session;        /// 会话 id
    int tty_nr;         /// 控制终端
    int tpgid;          /// 前台进程组 id
    int flags;          /// 进程标志

    long minflt;        /// 进程未发生页面错误数量（minor faults，即所需页面已在内存中）
    long cminflt;       /// 子进程 minflt 的总和
    long majflt;        /// 进程发生了多少次缺页错误（major faults，即所需页面不在内存中）
    long cmajflt;       /// 子进程 majflt 的总和

    long utime;         /// 用户模式下该进程被调度的时间
    long stime;         /// 内核模式下该进程被调度的时间
    long cutime;        /// 用户模式下该进程的子进程被调度的时间
    long cstime;        /// 内核模式下该进程的子进程被调度的时间

    long priority;      /// 优先级
    long nice;          /// nice 值
    long num_threads;   /// 进程创建的线程数
    long itrealvalue;   /// 下次将发送 SIGALRM 信号到进程的时间值
    long starttime;     /// 启动时间

    long vsizeKb;       /// 虚拟内存大小
    long rssKb;         /// 驻留集大小（Resident Set Size，它包含了进程所有的映射页，无论它们当前是否在内存中）
    long rsslim;        /// 进程可使用的物理内存最大值
};

class Procmon : public std::enable_shared_from_this<Procmon> {
public:
    typedef std::shared_ptr<Procmon> ptr;

    Procmon(pid_t pid, sylar::http::HttpServer::ptr serv, const std::string& procname) 
        : m_kClockTicksPerSecond(ProcessInfo::clockTicksPerSecond())
        , m_kbPerPage(ProcessInfo::pageSize() / 1024)
        , m_kBootTime(getBootTime())
        , m_pid(pid)
        , m_server(serv)
        , m_procname(procname)
        , m_hostname(ProcessInfo::hostname())
        , m_cmdline(getCmdLine())
        , m_ticks(0)
        , m_cpuUsage(600 / m_kPeriod)
        , m_cpuChart(640, 100, 600, m_kPeriod)
        , m_ramChart(640, 100, 7200, 30)
    {
        
        {
            // chdir to the same cwd of the process being monitored.
            // 应该获取符号链接 exe 所指向的可执行文件的绝对路径
            std::string exe = readLink("exe");
            auto pos = exe.find_last_of("/");
            exe = exe.substr(0, pos + 1);
            if (::chdir(exe.c_str())) {
                std::string error = "cannot to chdir() to " + exe;
                perror(error.c_str());
            }
        }

        {
            char cwd[1024] = {0};
            if (::getcwd(cwd, sizeof(cwd))) {
                // std::cout << "current dir: " << cwd << std::endl; 
                SYLAR_LOG_DEBUG(g_logger) << "current dir: " << cwd;
            }
        }

        bzero(&m_lastStatData, sizeof(m_lastStatData));
    } 

    void start() {
        tick();  // TODO
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        auto self = shared_from_this();
        iom->addConditionTimer(m_kPeriod * 1000, std::bind(&Procmon::tick, this), self, true);
        // server_.getLoop()->runEvery(kPeriod_, std::bind(&Procmon::tick, this));
        addServlets();
        m_server->start();
    }

public:
    void addHeaders(sylar::http::HttpResponse::ptr rsp) {
        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setHeader("content-type", "text/plain");
        rsp->setHeader("Server", "Procmon-Server");
    }

    void addServlets() {
        auto sld = m_server->getServletDispatch();
        auto self = shared_from_this();
        sld->addServlet("/", [self] (sylar::http::HttpRequest::ptr req
                                        , sylar::http::HttpResponse::ptr rsp
                                        , sylar::http::HttpSession::ptr session) {
            self->addHeaders(rsp);
            rsp->setHeader("content-type", "text/html");
            self->fillOverview(req->getQuery());
            rsp->setBody(self->m_response.str());
            // self->onRequest(req, rsp);
            return 0;
        });

        sld->addServlet("/cmdline", [self] (sylar::http::HttpRequest::ptr req
                                        , sylar::http::HttpResponse::ptr rsp
                                        , sylar::http::HttpSession::ptr session) {
            self->addHeaders(rsp);
            rsp->setBody(self->m_cmdline);
            return 0;
        });

        sld->addServlet("/cpu.png", [self] (sylar::http::HttpRequest::ptr req
                                        , sylar::http::HttpResponse::ptr rsp
                                        , sylar::http::HttpSession::ptr session) {
            self->addHeaders(rsp);
            rsp->setHeader("content-type", "image/png");
            std::vector<double> cpu_usage;
            for (size_t i = 0; i < self->m_cpuUsage.size(); ++i) {
                cpu_usage.push_back(self->m_cpuUsage[i].cpuUsage(self->m_kPeriod, self->m_kClockTicksPerSecond));
            }
            std::string png = self->m_cpuChart.plotCpu(cpu_usage);
            rsp->setBody(png);
            return 0;
        });
        
        sld->addServlet("/environ", [self] (sylar::http::HttpRequest::ptr req
                                    , sylar::http::HttpResponse::ptr rsp
                                    , sylar::http::HttpSession::ptr session) {
                self->addHeaders(rsp);
                rsp->setBody(self->getEnviron());
                return 0;
        });

        sld->addServlet("/files", [self] (sylar::http::HttpRequest::ptr req
                                        , sylar::http::HttpResponse::ptr rsp
                                        , sylar::http::HttpSession::ptr session) {
            self->addHeaders(rsp);
            self->listFiles();
            rsp->setBody(self->m_response.str());
            return 0;
        });

        sld->addServlet("/threads", [self] (sylar::http::HttpRequest::ptr req
                                        , sylar::http::HttpResponse::ptr rsp
                                        , sylar::http::HttpSession::ptr session) {
            self->addHeaders(rsp);
            self->listThreads();
            rsp->setBody(self->m_response.str());
            return 0;
        });    

        for (auto& path: s_paths) {
            std::string p = path.substr(1, path.size() - 1);
            sld->addServlet(path, [p, self] (sylar::http::HttpRequest::ptr req
                                        , sylar::http::HttpResponse::ptr rsp
                                        , sylar::http::HttpSession::ptr session) {
                self->addHeaders(rsp);
                rsp->setBody(self->readProcFile(p));
                return 0;
            });
        }    
    }

    std::string getName() {
        std::string str = "procmon-" + std::to_string(m_pid);
        return str;
    }

    #if 0
    void onRequest(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp) {
        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setHeader("content-type", "text/plain");
        rsp->setHeader("Server", "Procmon-Server");

        if (req->getPath() == "/") {
            rsp->setHeader("content-type", "text/html");
            fillOverview(req->getQuery());
            rsp->setBody(m_response.str());
        } else if (req->getPath() == "/cmdline") {
            rsp->setBody(m_cmdline);
        } else if (req->getPath() == "/cpu.png") {
            std::vector<double> cpu_usage;
            for (size_t i = 0; i < cpu_usage.size(); ++i) {
                cpu_usage.push_back(m_cpuUsage[i].cpuUsage(m_kPeriod, m_kClockTicksPerSecond));
            }
            std::string png = m_cpuChart.plotCpu(cpu_usage);
            rsp->setHeader("content-type", "image/png");
            rsp->setBody(png);
        } else if (req->getPath() == "/environ") {
            rsp->setBody(getEnviron());
        } else if (req->getPath() == "/io") {
            rsp->setBody(readProcFile("io"));
        } else if (req->getPath() == "/limits") {
            rsp->setBody(readProcFile("limits"));
        } else if (req->getPath() == "/maps") {
            rsp->setBody(readProcFile("maps"));
        } else if (req->getPath() == "/smaps") {  // numa_maps
            rsp->setBody(readProcFile("smaps"));
        } else if (req->getPath() == "/status") {
            rsp->setBody(readProcFile("status"));
        } else if (req->getPath() == "/files") {
            listFiles();
            rsp->setBody(m_response.str());
        } else if (req->getPath() == "/threads") {
            listThreads();
            rsp->setBody(m_response.str());
        } else {
            rsp->setStatus(sylar::http::HttpStatus::NOT_FOUND);
            rsp->setClose(true);
        }
    }
    #endif

    void fillOverview(const std::string& query) {
        // m_response.reset(new sylar::ByteArray);
        m_response.str("");
        m_response.clear();
        sylar::Timestamp now = sylar::Timestamp::now();
        // SYLAR_LOG_DEBUG(g_logger) << m_procname << " " << m_hostname << std::endl;

        appendResponse("<html><head><title>%s on %s</title>\n",
                    m_procname.c_str(), m_hostname.c_str());
        
        fillRefresh(query);
        appendResponse("</head><body>\n");

        std::string stat = readProcFile("stat");
        if (stat.empty())
        {
            appendResponse("<h1>PID %d doesn't exist.</h1></body></html>", m_pid);
            return;
        }
        
        int pid = atoi(stat.c_str());
        assert(pid == m_pid);
        
        appendResponse("<h1>%s on %s</h1>\n",
                    m_procname.c_str(), m_hostname.c_str());
        
        m_response << "<p>Refresh <a href=\"?refresh=1\">1s</a> "
                       << "<a href=\"?refresh=2\">2s</a> "
                       << "<a href=\"?refresh=5\">5s</a> "
                       << "<a href=\"?refresh=15\">15s</a> "
                       << "<a href=\"?refresh=60\">60s</a>\n"
                       << "<p><a href=\"/cmdline\">Command line</a>\n"
                       << "<a href=\"/environ\">Environment variables</a>\n"
                       << "<a href=\"/threads\">Threads</a>\n";

        appendResponse("<p>Page generated at %s (UTC)", now.toFormattedString(false).c_str());

        m_response << "<p><table>";

        StatData statData;  // how about use lastStatData_ ?
        bzero(&statData, sizeof(statData));

        StringPiece procname = ProcessInfo::procname(stat);
        statData.parse(procname.end() + 1, m_kbPerPage);

        appendTableRow("PID", pid);
        sylar::Timestamp started(getStartTime(statData.starttime));  // FIXME: cache it;
        appendTableRow("Started at", started.toFormattedString(false) + " (UTC)");
        appendTableRow("Uptime (s)", timeDifferenceFormat(now, started));  // format as days+H:M:S
        appendTableRow("Executable", readLink("exe"));
        appendTableRow("Current dir", readLink("cwd"));

        appendTableRow("State", getState(statData.state));
        appendTableRowFloat("User time (s)", getSeconds(statData.utime));
        appendTableRowFloat("System time (s)", getSeconds(statData.stime));

        appendTableRow("VmSize (KiB)", statData.vsizeKb);
        appendTableRow("VmRSS (KiB)", statData.rssKb);
        appendTableRow("Threads", statData.num_threads);
        appendTableRow("CPU usage", "<img src=\"/cpu.png\" height=\"100\" witdh=\"640\">");

        appendTableRow("Priority", statData.priority);
        appendTableRow("Nice", statData.nice);

        appendTableRow("Minor page faults", statData.minflt);
        appendTableRow("Major page faults", statData.majflt);
        // TODO: user

        m_response << "</table>"
            << "</body></html>";
    }

    void fillRefresh(const std::string& query) {
        auto p = query.find("refresh=");
        if (p != std::string::npos) {
            int seconds = atoi(query.c_str()+p+8);
            if (seconds > 0) {
                appendResponse("<meta http-equiv=\"refresh\" content=\"%d\">\n", seconds);
            }
        }
    }

    static int dirFilter(const struct dirent* d) {
        return (d->d_name[0] != '.');
    }

    static char getDirType(char d_type) {
        switch (d_type)
        {
        case DT_REG: return '-';
        case DT_DIR: return 'd';
        case DT_LNK: return 'l';
        default: return '?';
        }
    }

    void listFiles() {
        m_response.str("");
        m_response.clear();
        struct dirent** namelist = nullptr;
        int result = ::scandir(".", &namelist, dirFilter, alphasort);
        for (int i = 0; i < result; ++i) {
            struct stat stat;
            if (::lstat(namelist[i]->d_name, &stat) == 0) {
                sylar::Timestamp mtime(stat.st_mtime * sylar::Timestamp::kMicroSecondsPerSecond);
                int64_t size = stat.st_size;
                appendResponse("%c %9" PRId64 " %s %s", getDirType(namelist[i]->d_type)
                            , size, mtime.toFormattedString(false).c_str(), namelist[i]->d_name);
                if (namelist[i]->d_type == DT_LNK) {
                    char link[1024];
                    ssize_t len = ::readlink(namelist[i]->d_name, link, sizeof(link) - 1);
                    if (len > 0) {
                        link[len] = '\0';
                        appendResponse(" -> %s", link);
                    }
                }
                appendResponse("\n");
            }
            ::free(namelist[i]);
        }
        ::free(namelist);
    }

    void listThreads() {
        // m_response.reset(new sylar::ByteArray);
        m_response.str("");
        m_response.clear();
        // FIXME: implement this
    }

    std::string readProcFile(const std::string& basename) {
        std::string filename =  "/proc/" +  std::to_string(m_pid) + "/" + basename;
        std::string content;
        FileUtil::readFile(filename, 1024 * 1024, &content);
        return content;
    }

    std::string readLink(const std::string& basename) {
        std::string filename =  "/proc/" +  std::to_string(m_pid) + "/" + basename;
        char link[1024] = {0};
        ssize_t len = ::readlink(filename.c_str(), link, sizeof(link));
        std::string res;
        if (len > 0) {
            res.assign(link, len);
        }
        return res;
    }

    int appendResponse(const char* fmt, ...);

    void appendTableRow(const std::string& name, long value) {
        appendResponse("<tr><td>%s</td><td>%ld</td></tr>\n", name.c_str(), value);
    }

    void appendTableRowFloat(const std::string& name, double value) {
        appendResponse("<tr><td>%s</td><td>%.2f</td></tr>\n", name.c_str(), value);
    }

    void appendTableRow(const std::string& name, const std::string& value) {
        appendResponse("<tr><td>%s</td><td>%s</td></tr>\n", name.c_str(), value.c_str());
    }

    std::string getCmdLine() {
        /// /proc/pid/cmdline
        return boost::replace_all_copy(readProcFile("cmdline"), std::string(1, '\0'), "\n\t");
    }

    std::string getEnviron() {
        /// /proc/pid/environ
        return boost::replace_all_copy(readProcFile("environ"), std::string(1, '\0'), "\n");
    }

    sylar::Timestamp getStartTime(long starttime)
    {
        return sylar::Timestamp(sylar::Timestamp::kMicroSecondsPerSecond * m_kBootTime
                        + sylar::Timestamp::kMicroSecondsPerSecond * starttime / m_kClockTicksPerSecond);
    }

    double getSeconds(long ticks)
    {
        return static_cast<double>(ticks) / m_kClockTicksPerSecond;
    }

    /// 周期性 tick()，即周期性采样，收集 CPU 使用率
    void tick() {
        std::string stat = readProcFile("stat");
        if (stat.empty()) {
            return;
        }

        StringPiece procname = ProcessInfo::procname(stat);
        StatData sd;
        bzero(&sd, sizeof(sd));
        sd.parse(procname.end() + 1, m_kbPerPage);  // end is ')'
        // sd.parse(stat, m_kbPerPage);  // 直接解析 string 类型的 stat
        if (m_ticks > 0) {
            CpuTime time;
            time.userTime = std::max(0, static_cast<int>(sd.utime - m_lastStatData.utime));
            time.sysTime = std::max(0, static_cast<int>(sd.stime - m_lastStatData.stime));
            m_cpuUsage.push_back(time);
        }

        m_lastStatData = sd;
        ++m_ticks;
    }

    /// static member function
    static std::string getState(char state) {
        switch (state) {
            case 'R':
                return "Running";
            case 'S':
                return "Sleeping";
            case 'D':
                return "Disk sleep";
            case 'Z':
                return "Zombie";
            default:
                return "Unknown";
        }
    }

    static long getLong(const std::string& status, const char* key) {
        long result = 0;
        size_t pos = status.find(key);
        if (pos != std::string::npos) {
            result = ::atol(status.c_str() + pos + strlen(key));
        }
        return result;
    }

    static long getBootTime() {
        std::string stat;
        FileUtil::readFile("/proc/stat", 65536, &stat);
        return getLong(stat, "btime ");
    }

    struct CpuTime {
        int userTime;
        int sysTime;
        double cpuUsage(double kPeriod, double kClockTicksPerSecond) const {
            return (userTime + sysTime) / (kClockTicksPerSecond * kPeriod);
        }
    };

private:
    const static int m_kPeriod = 2.0;
    const int m_kClockTicksPerSecond;
    const int m_kbPerPage;
    const long m_kBootTime;
    const pid_t m_pid;
    sylar::http::HttpServer::ptr m_server;
    const std::string m_procname;
    const std::string m_hostname;
    const std::string m_cmdline;
    int m_ticks;
    StatData m_lastStatData;
    boost::circular_buffer<CpuTime> m_cpuUsage;
    Plot m_cpuChart;
    Plot m_ramChart;
    // scratch variables
    std::stringstream m_response;
    // sylar::ByteArray::ptr m_response;
    static std::vector<std::string> s_paths;
};
std::vector<std::string> Procmon::s_paths = {"/io", "/limits", "/maps", "/smaps", "/status"};

int Procmon::appendResponse(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    m_response << buf;
    return ret;
}

bool ProcessExists(pid_t pid) {
    std::string filename = "/proc/" + std::to_string(pid) + "/stat";
    return ::access(filename.c_str(), R_OK) == 0;    
}

void run_procmon(pid_t pid, uint16_t port, const std::string& proc_name) {
    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("0.0.0.0:" + std::to_string(port));
    sylar::http::HttpServer::ptr server(new sylar::http::HttpServer(true));
    while (!server->bind(addr)) {
        perror("failed to bind");
        sleep(1);
    }    
    std::string procname = proc_name.empty() ? ProcessInfo::procname() : proc_name;
    SYLAR_LOG_DEBUG(g_logger) << procname;

    auto logger = SYLAR_LOG_NAME("system");
    logger->setLevel(sylar::LogLevel::ERROR);
    
    Procmon::ptr procmon(new Procmon(pid, server, procname));
    procmon->start();

}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " [pid] [port] [name]" << std::endl;
        return 0;
    }
    int pid = atoi(argv[1]);
    if (!ProcessExists(pid)) {
        std::cout << "Process " << pid << " doesn't exist." << std::endl;
        return 1;
    }
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    std::string proc_name = argc > 3 ? argv[3] : "";

    sylar::IOManager iom;
    iom.schedule(std::bind(run_procmon, pid, port, proc_name));
    return 0;
}
