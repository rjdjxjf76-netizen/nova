#pragma once
#include <string>
#include <vector>

// Supports the local NOVA format {"items":[...]} and the GAMES.json
// metadata format {"DATA":{"package-url":{"name", "title_id", ...}}}.
// DATA package URL keys are intentionally ignored; this importer is metadata-only.
struct CatalogEntry {
    std::string name, type, version, size, publisher, description, url, sha256;
    std::string title_id, region, min_fw, release, cover_url;
};
bool catalog_parse(const std::string& json, std::vector<CatalogEntry>& out, std::string& err);
bool catalog_load_file(const char* path, std::vector<CatalogEntry>& out, std::string& err);
