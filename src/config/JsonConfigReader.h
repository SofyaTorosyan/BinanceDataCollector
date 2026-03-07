#pragma once
#include "IConfigReader.h"
#include <string_view>

namespace bdc::config
{

class JsonConfigReader : public IConfigReader
{
public:
    explicit JsonConfigReader(std::string_view filePath);

    AppConfig getConfig() const override;

private:
    AppConfig load(std::string_view filePath);

    AppConfig m_config;
};

} // namespace bdc::config
