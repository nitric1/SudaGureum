#include "Common.h"

#include "Global.h"

namespace Batang
{
    Environment::Environment()
    {
        inst_ = GetModuleHandle(nullptr);
    }

    GlobalInitializerManager::~GlobalInitializerManager()
    {
        for(auto it = std::begin(inits); it != std::end(inits); ++ it)
            it->second->uninit();
    }

    void GlobalInitializerManager::add(const std::shared_ptr<Initializer> &init)
    {
        std::lock_guard<std::mutex> lg(lock);
        if(inits.find(init->getName()) != std::end(inits))
            return;
        inits.insert(std::make_pair(init->getName(), init));
        init->init();
    }
}
