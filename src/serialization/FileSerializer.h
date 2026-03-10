#pragma once

#include "AppConfig.h"
#include "ISerializer.h"

namespace bdc::serialization
{

class FileSerializer : public ISerializer
{
public:
    explicit FileSerializer(config::AppConfigPtr config);
    void write(const std::vector<WindowStats>& windows) override;

private:
    config::AppConfigPtr m_config;
};

} // namespace bdc::serialization
