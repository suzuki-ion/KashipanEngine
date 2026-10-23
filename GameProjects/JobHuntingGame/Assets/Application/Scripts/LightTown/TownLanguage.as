string TownTr(const string &in ja, const string &in en) {
    return GetCurrentLanguage() == "ja-JP" ? ja : en;
}
string TownBuildingName(Json@ definition) {
    return GetCurrentLanguage() == "ja-JP" ? definition.GetString("name") : definition.GetString("nameEn", definition.GetString("name"));
}
