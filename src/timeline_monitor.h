#pragma once

#include <atomic>
#include <chrono>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace timeline_monitor {

class TimelineElem {
public:
    enum Type {
        /**
         * Beginning of a timeline event.
         */
        BEGIN = 0,

        /**
         * End of a timeline event.
         */
        END   = 1
    };

    /**
     * Default constructor.
     *
     * @param name Name of timeline event.
     * @param t Type.
     * @param i ID.
     * @param d Depth.
     */
    TimelineElem(const char* name, Type t, uint64_t i, uint32_t d)
        : timelineName(name)
        , type(t)
        , id(i)
        , depth(d)
        , timestamp(std::chrono::system_clock::now())
        {}

    /**
     * Get the name of the element.
     *
     * @return Name.
     */
    inline const char* getName() const { return timelineName; }

    /**
     * Get the ID of the element.
     *
     * @return ID.
     */
    inline uint64_t getId() const { return id; }

    /**
     * Get the type of the element.
     *
     * @return Type.
     */
    inline Type getType() const { return type; }

    /**
     * Get the timestamp of the element.
     *
     * @return Timestamp.
     */
    inline std::chrono::system_clock::time_point getTimestamp() const {
        return timestamp;
    }

    /**
     * Convert the given `time_point` to an epoch in microseconds.
     *
     * @param tp Time point.
     * @return Epoch in microseconds.
     */
    static uint64_t getEpochUs(const std::chrono::system_clock::time_point& tp) {
        return std::chrono::duration_cast< std::chrono::microseconds >(
            tp.time_since_epoch() ).count();
    }

    /**
     * Get the timestamp of the element as an epoch.
     *
     * @return Epoch in microseconds.
     */
    inline uint64_t getEpochUs() const {
        return getEpochUs(timestamp);
    }

    /**
     * Get the depth of the element.
     *
     * @return Depth.
     */
    inline uint32_t getDepth() const { return depth; }

private:
    /**
     * Name of timeline event.
     */
    const char* timelineName;

    /**
     * Type.
     */
    Type type;

    /**
     * ID.
     */
    uint64_t id;

    /**
     * Depth.
     */
    uint32_t depth;

    /**
     * Timestamp.
     */
    std::chrono::system_clock::time_point timestamp;
};

class Timeline {
public:
    /**
     * Default constructor.
     */
    Timeline() : lastDepth(0) {}

    /**
     * Append a `BEGIN` event.
     *
     * @param name Name of the event.
     * @param id ID.
     */
    void pushBegin(const char* name, uint64_t id) {
        elems.push_back( TimelineElem(name, TimelineElem::BEGIN, id, lastDepth++) );
    }

    /**
     * Append an `END` event.
     * If the current event corresponds to the first `BEGIN` event
     * of this timeline, the contents of this timeline
     * will be reset.
     *
     * @param name Name of the event, should match that of
     *             corresponding `BEGIN` event.
     * @param id ID, should match that of the corresponding
     *           `BEGIN` event.
     */
    void pushEnd(const char* name, uint64_t id) {
        elems.push_back( TimelineElem(name, TimelineElem::END, id, --lastDepth) );
        clearIfNecessary(id);
    }

    /**
     * Append an `END` event, and export the current timeline
     * instance.
     * After export, the contents of this timeline will be reset.
     *
     * @param name Name of the event, should match that of
     *             corresponding `BEGIN` event.
     * @param id ID, should match that of the corresponding
     *           `BEGIN` event.
     * @return Exported timeline instance.
     */
    Timeline pushEndAndExport(const char* name, uint64_t id) {
        elems.push_back( TimelineElem(name, TimelineElem::END, id, --lastDepth) );
        Timeline ret = *this;
        clearIfNecessary(id);
        return ret;
    }

    /**
     * Check if the current event corresponds to the the first
     * `BEGIN` event of this timeline. If so, reset the contents
     * of the current timeline.
     *
     * @param id ID.
     */
    void clearIfNecessary(uint64_t id) {
        TimelineElem& first_elem = *elems.begin();
        if (first_elem.getId() == id) {
            clear();
        }
    }

    /**
     * Get the current elements.
     *
     * @return Reference to the list of elements.
     */
    const std::list<TimelineElem>& getElems() const { return elems; }

private:
    void clear() {
        elems.clear();
    }

    /**
     * List of timeline elements.
     */
    std::list<TimelineElem> elems;

    /**
     * Stack depth for next BEGIN event.
     */
    uint32_t lastDepth;
};

/**
 * This class is an example of timeline dump. Users can write their
 * own dump class.
 */
class TimelineDump {
public:
    /**
     * Dump the timeline to string.
     */
    static std::string toString(const Timeline& src) {
        std::stringstream ret;
        std::unordered_map<uint64_t, TimelineElem*> begin_elems;
        auto elems = src.getElems();

        bool need_endl = false;
        for (auto& entry: elems) {
            const TimelineElem& cur_elem = entry;
            if (cur_elem.getType() == TimelineElem::BEGIN) {
                begin_elems.insert( std::make_pair( cur_elem.getId(),
                                                    (TimelineElem*)&cur_elem ) );
                if (need_endl) {
                    ret << std::endl;
                    need_endl = false;
                }
                ret << std::string(cur_elem.getDepth(), ' ')
                    << cur_elem.getName() << ", "
                    << cur_elem.getEpochUs();
                need_endl = true;
                continue;
            } else {
                auto e_begin = begin_elems.find(cur_elem.getId());
                if (e_begin == begin_elems.end()) {
                    // Something went wrong, ignore it.
                    continue;
                }
                const TimelineElem& begin_elem = *(e_begin->second);
                if (need_endl) {
                    ret << std::endl;
                    need_endl = false;
                }
                ret << std::string(begin_elem.getDepth(), ' ')
                    << begin_elem.getName() << ", "
                    << cur_elem.getEpochUs() << ", "
                    << cur_elem.getEpochUs() - begin_elem.getEpochUs();
                need_endl = true;
            }
        }
        return ret.str();
    }
};

/**
 * Counter for assigning ID of events for each thread.
 */
static thread_local uint64_t event_id_counter(1);

/**
 * Timeline of each thread.
 */
static thread_local Timeline thread_events;

/**
 * Automatic monitoring class.
 */
class TimelineMonitor {
public:
    /**
     * Constructor for using the thread's default timeline.
     *
     * @param name Name of event (function or block name).
     */
    TimelineMonitor(const char* name)
        : timelineRef(thread_events)
        , myName(name)
        , myId(event_id_counter++)
        , done(false)
    {
        timelineRef.pushBegin(myName, myId);
    }

    /**
     * Constructor for using the given custom timeline.
     *
     * @param timeline Custom timeline.
     * @param name Name of event (function or block name).
     */
    TimelineMonitor(Timeline& timeline, const char* name)
        : timelineRef(timeline)
        , myName(name)
        , myId(event_id_counter++)
        , done(false)
    {
        timelineRef.pushBegin(myName, myId);
    }

    /**
     * Destructor.
     */
    ~TimelineMonitor() {
        if (!done) {
            timelineRef.pushEnd(myName, myId);
            done = true;
        }
    }

    /**
     * Export the current timeline.
     *
     * @return Exported timeline instance.
     */
    Timeline exportTimeline() {
        Timeline ret = timelineRef.pushEndAndExport(myName, myId);
        done = true;
        return ret;
    }

    /**
     * Get the elapsed time since the beginning of monitoring
     * in microseconds.
     *
     * @return Elapsed time in microseconds.
     */
    uint64_t getElapsedUs() const {
        const std::list<TimelineElem>& elems = timelineRef.getElems();
        auto entry = elems.begin();
        if (entry == elems.end()) return 0;

        const TimelineElem& te = *entry;
        return TimelineElem::getEpochUs(std::chrono::system_clock::now()) -
               te.getEpochUs();
    }

private:
    /**
     * Reference to timeline instance.
     */
    Timeline& timelineRef;

    /**
     * Name of the current event (function or block name).
     */
    const char* myName;

    /**
     * ID of the current event.
     */
    uint64_t myId;

    /**
     * `true` if the monitoring of current event is done.
     */
    bool done;
};

//#define TIMELINE_MONITOR_DISABLED
#ifndef TIMELINE_MONITOR_DISABLED

#define T_MONITOR_FUNC() \
    timeline_monitor::TimelineMonitor __auto_monitor(__func__)

#define T_MONITOR_BLOCK(name) \
    timeline_monitor::TimelineMonitor __auto_monitor(name)

#define T_MONITOR_FUNC_CUSTOM(custom_timeline) \
    timeline_monitor::TimelineMonitor __auto_monitor(custom_timeline, __func__)

#define T_MONITOR_BLOCK_CUSTOM(custom_timeline, name) \
    timeline_monitor::TimelineMonitor __auto_monitor(custom_timeline, name)

#define T_MONITOR_EXPORT() __auto_monitor.exportTimeline()

#define T_MONITOR_ELAPSED_TIME() __auto_monitor.getElapsedUs()

#else
// Disabled

#define T_MONITOR_FUNC()

#define T_MONITOR_BLOCK(name)

#define T_MONITOR_FUNC_CUSTOM(custom_timeline)

#define T_MONITOR_BLOCK_CUSTOM(custom_timeline, name)

#define T_MONITOR_EXPORT() timeline_monitor::Timeline()

#define T_MONITOR_ELAPSED_TIME() (0)

#endif

} // namespace timeline_monitor;

