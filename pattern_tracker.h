extern int pattern_tracker_stop;
extern pthread_mutex_t pattern_tracker_lock;
extern pthread_cond_t pattern_tracker_cond;
extern unsigned long pattern_duration;
extern short int agios_is_pattern_tracking;

inline void stop_the_pattern_tracker_thread();
inline void add_request_to_pattern(unsigned long int timestamp, unsigned long int offset, unsigned long int len);
int pattern_tracker_init();


