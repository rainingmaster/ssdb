#ifndef UTIL_APP_H
#define UTIL_APP_H

#include <string>

class Config;

class Application{
public:
	Application(){};
	virtual ~Application(){};
    
    // 相当于main函数，程序入口
	int main(int argc, char **argv);

    // 输出帮助
	virtual void usage(int argc, char **argv);
    // 输出欢迎信息
	virtual void welcome() = 0;
    // 具体的启动执行流程
	virtual void run() = 0;

protected:
	struct AppArgs{
        // 是否以守护进程启动
		bool is_daemon;

		std::string pidfile;
		std::string conf_file;
		std::string work_dir;
		std::string start_opt;

        //默认值
		AppArgs(){
			is_daemon = false;
			start_opt = "start";
		}
	};

	Config *conf;
	AppArgs app_args;
	
private:
	void parse_args(int argc, char **argv);
	void init();

	int read_pid();
	void write_pid();
	void check_pidfile();
	void remove_pidfile();
	void kill_process();
};

#endif
