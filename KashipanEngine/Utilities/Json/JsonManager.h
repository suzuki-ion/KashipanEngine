#pragma once
#include <memory>
#include "Data/JsonOutput.h"
#include "Data/JsonInput.h"
#include <string>
#include <vector>

//std::vectorと、std::stringやstd::vectorを含むクラス以外の型に対応

/// <summary>
/// Assets/Jsonの中身をJSON形式で色々するクラス
/// </summary>
class JsonManager {
public:

	JsonManager();
	~JsonManager();

	template<typename T>
	void RegistOutput(T value, std::string name = "") {
		values.push_back(std::make_shared<Value<T>>(value, name));
	};

	template<typename T>
	static T Reverse(std::shared_ptr<ValueBase> value) {
		return static_cast<Value<T>*>(value.get())->value;
	}

	/// <summary>
	/// 実行後RegistしたValueはflushします
	/// </summary>
	void Write(std::string fileName);

	/// <summary>
	/// 指定したファイルからValueを作成します
	/// </summary>
	/// <returns>新たに生成したValue配列</returns>
	std::vector<std::shared_ptr<ValueBase>> Read(std::string fileName);

	/// <summary>
	/// データの保存先パスを変更する
	/// </summary>
	/// <param name="path"></param>
	void SetBasePath(std::string path) {
		basePath = path;
	}

	/// <summary>
	/// データの保存先パスを取得する
	/// </summary>
	/// <returns>現在のベースパス</returns>
	std::string GetBasePath() const {
		return basePath;
	}

private:

	std::unique_ptr<JsonInput> input;
	std::unique_ptr<JsonOutput> output;
	std::vector<std::shared_ptr<ValueBase>> values;

	std::string basePath = "resources/Data/Json/"; // データの保存先パス（インスタンスごと）

};