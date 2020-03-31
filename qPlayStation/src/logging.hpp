#pragma once
#include <string>

const std::string sourceNames[] = { "qPlayStation", "CPU", "Memory", "BIOS", "DMA", "GPU", "CDROM", "GTE", "TTY", "?" };

class logging
{
    public:
        enum class logSource
        {
            qPS,
            CPU,
            memory,
            BIOS,
            DMA,
            GPU,
            CDROM,
            GTE,
            TTY,
            unknown
        };
        static void info(std::string toLog, logSource source = logSource::unknown);
        static void important(std::string toLog, logSource source = logSource::unknown);
        static void warning(std::string toLog, logSource source = logSource::unknown);
        static void error(std::string toLog, logSource source = logSource::unknown);
        static void fatal(std::string toLog, logSource source = logSource::unknown);
    private:
        //private constructor means no instances of this object can be created
        logging() {}
		static std::string format(std::string toLog, std::string level, std::string color, logSource source);
};