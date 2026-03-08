#pragma once

#include "ISerializer.h"
#include <string>

namespace bdc::serialization
{

class FileSerializer : public ISerializer
{
public:
    explicit FileSerializer(std::string filePath);
    void write(const std::vector<WindowStats>& windows) override;

private:
    std::string m_filePath;
};

} // namespace bdc::serialization
