#pragma once

#include "CDXShaderFactory.h"
#include <string>
#include <vector>

class CDXBSShaderResource : public CDXShaderFile
{
public:
	CDXBSShaderResource(const char * filePath, const char * entryPoint = "");

	virtual void* GetBuffer() const;
	virtual size_t GetBufferSize() const;
	virtual const char* GetSourceName() const;
	virtual const char* GetEntryPoint() const;

private:
	std::string			m_entryPoint;
	std::string			m_sourceName;
	std::vector<char>	m_fileBuffer;
};