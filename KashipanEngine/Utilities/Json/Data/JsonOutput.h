#pragma once
#include <ostream>
#include <string>
#include <vector>
#include <memory>
#include "Value.h"

class JsonOutput {
public:

	void WriteJson(std::ostream& out, const std::vector<std::shared_ptr<ValueBase>>& values);

private:
	void WriteValue(std::ostream& out, const ValueBase* value, bool isLast);
	std::string EscapeString(const std::string& str);
};