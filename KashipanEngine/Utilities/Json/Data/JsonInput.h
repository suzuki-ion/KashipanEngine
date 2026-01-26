#pragma once
#include <istream>
#include <vector>
#include <memory>
#include "Value.h"

class JsonInput {
public:

	std::vector<std::shared_ptr<ValueBase>> ReadJson(std::istream& in);

private:
	std::string ReadString(std::istream& in);
	void SkipWhitespace(std::istream& in);
	void SkipUntil(std::istream& in, char target);
	bool ExpectChar(std::istream& in, char expected);
	std::shared_ptr<ValueBase> ParseValue(std::istream& in);
};