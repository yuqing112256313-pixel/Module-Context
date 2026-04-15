#include "core/framework/context.h"

#include "core/framework/module_manager.h"

#include <fstream>
#include <string>

namespace mc {

namespace {

std::string trim(const std::string& value)
{
    std::size_t begin = 0;
    while (begin < value.size()
           && (value[begin] == ' '
               || value[begin] == '\t'
               || value[begin] == '\r'
               || value[begin] == '\n')) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin
           && (value[end - 1] == ' '
               || value[end - 1] == '\t'
               || value[end - 1] == '\r'
               || value[end - 1] == '\n')) {
        --end;
    }

    return value.substr(begin, end - begin);
}

bool parseLine(const std::string& line,
               std::string& name,
               std::string& libraryPath)
{
    const std::size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return false;
    }

    name = trim(line.substr(0, pos));
    libraryPath = trim(line.substr(pos + 1));

    return !name.empty() && !libraryPath.empty();
}

} // namespace

Context::Context()
    : moduleManager_(new ModuleManager())
    , state_(ModuleState::Created)
{
}

Context::~Context()
{
    if (state_ != ModuleState::Fini) {
        fini();
    }
}

bool Context::loadModules(const std::string& configFilePath)
{
    std::ifstream ifs(configFilePath.c_str());
    if (!ifs.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::string name;
        std::string libraryPath;
        if (!parseLine(line, name, libraryPath)) {
            return false;
        }

        if (!moduleManager_->loadModule(name, libraryPath)) {
            return false;
        }
    }

    return true;
}

void Context::init()
{
    moduleManager_->init(*this);
    state_ = ModuleState::Inited;
}

void Context::start()
{
    moduleManager_->start();
    state_ = ModuleState::Started;
}

void Context::stop()
{
    moduleManager_->stop();
    state_ = ModuleState::Stopped;
}

void Context::fini()
{
    moduleManager_->fini();
    state_ = ModuleState::Fini;
}

ModuleState Context::state() const
{
    return state_;
}

IModuleManager* Context::moduleMgr()
{
    return moduleManager_.get();
}

} // namespace mc