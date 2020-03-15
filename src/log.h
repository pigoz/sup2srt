#include <libavutil/avutil.h>

#define log(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)
#define warn(...) av_log(NULL, AV_LOG_WARNING, __VA_ARGS__)
#define err(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__) 
