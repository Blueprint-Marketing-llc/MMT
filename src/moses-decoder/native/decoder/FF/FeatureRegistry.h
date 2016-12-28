#pragma once

#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#include <mmt/logging/Logger.h>

namespace Moses2 {
    class FeatureFunction;

////////////////////////////////////////////////////////////////////
    class FeatureFactory {
    public:
        virtual ~FeatureFactory() {
        }

        virtual FeatureFunction *Create(size_t startInd, const std::string &line) = 0;

    protected:
        FeatureFactory() {
        }
    };

////////////////////////////////////////////////////////////////////
    class FeatureRegistry {
    public:
        FeatureRegistry();

        ~FeatureRegistry();

        FeatureFunction *Construct(size_t startInd, const std::string &name,
                                   const std::string &line);

        void PrintFF() const;

    private:
        const mmt::logging::Logger logger;

        void Add(const std::string &name, FeatureFactory *factory);

        typedef boost::unordered_map <std::string, boost::shared_ptr<FeatureFactory>> Map;

        Map registry_;
    };

////////////////////////////////////////////////////////////////////

}

