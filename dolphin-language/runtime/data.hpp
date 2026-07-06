#pragma once
#include "builtins.hpp"

struct TemplateNamespace {
    var _getValue(const var& data, const std::string& path) {
        if (path.empty()) return var("");
        std::stringstream ss(path);
        std::string part;
        var current = data;
        while (std::getline(ss, part, '.')) {
            if (current.isObject() && current.has(part)) {
                current = current[part];
            } else {
                return var("");
            }
        }
        return current;
    }

    var render(const var& templateStr, const var& data) {
        std::string tpl = templateStr.toString();
        std::string result = "";
        size_t pos = 0;
        
        while (pos < tpl.length()) {
            size_t start = tpl.find("{{", pos);
            if (start == std::string::npos) {
                result += tpl.substr(pos);
                break;
            }
            
            result += tpl.substr(pos, start - pos);
            size_t end = tpl.find("}}", start);
            if (end == std::string::npos) {
                result += tpl.substr(start);
                break;
            }
            
            std::string tag = tpl.substr(start + 2, end - start - 2);
            tag.erase(tag.begin(), std::find_if(tag.begin(), tag.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            tag.erase(std::find_if(tag.rbegin(), tag.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), tag.end());
            
            if (tag.rfind("loop ", 0) == 0) {
                std::string loop_content = tag.substr(5);
                size_t as_pos = loop_content.find(" as ");
                if (as_pos == std::string::npos) {
                    pos = end + 2;
                    continue;
                }
                std::string container_name = loop_content.substr(0, as_pos);
                std::string item_name = loop_content.substr(as_pos + 4);
                
                size_t loop_end = tpl.find("{{endloop}}", end + 2);
                if (loop_end == std::string::npos) {
                    pos = end + 2;
                    continue;
                }
                
                std::string body = tpl.substr(end + 2, loop_end - end - 2);
                var container = _getValue(data, container_name);
                
                if (container.isArray()) {
                    for (int i = 0; i < container.size().toInt(); ++i) {
                        var local_data = data;
                        local_data[item_name] = container[i];
                        result += render(body, local_data).toString();
                    }
                }
                pos = loop_end + 11;
            }
            else if (tag.rfind("if ", 0) == 0) {
                std::string cond_name = tag.substr(3);
                size_t if_end = tpl.find("{{endif}}", end + 2);
                if (if_end == std::string::npos) {
                    pos = end + 2;
                    continue;
                }
                
                std::string body = tpl.substr(end + 2, if_end - end - 2);
                var cond = _getValue(data, cond_name);
                
                if (cond.toBool()) {
                    result += render(body, data).toString();
                }
                pos = if_end + 9;
            }
            else {
                var val = _getValue(data, tag);
                if (val.isNull()) {
                    if (data.isObject() && data.has(tag)) {
                        val = data[tag];
                    }
                }
                result += val.toString();
                pos = end + 2;
            }
        }
        return var(result);
    }

    var renderFile(const var& filePath, const var& data) {
        var content = File.read(filePath);
        if (content.isNull()) return var("");
        return render(content, data);
    }
} Template;

struct MatrixNamespace {
    var create(const var& rows, const var& cols, const var& data = var()) {
        return var::MatrixCreate(rows.toInt(), cols.toInt(), data);
    }
    var zeros(const var& rows, const var& cols) {
        return var::MatrixZeros(rows.toInt(), cols.toInt());
    }
    var ones(const var& rows, const var& cols) {
        return var::MatrixOnes(rows.toInt(), cols.toInt());
    }
    var random(const var& rows, const var& cols) {
        return var::MatrixRandom(rows.toInt(), cols.toInt());
    }
} Matrix;

struct MLNamespace {
    var dense(const var& inputs, const var& outputs) {
        var layer = var(var_object{});
        layer["weights"] = var::MatrixRandom(inputs.toInt(), outputs.toInt());
        layer["biases"] = var::MatrixZeros(1, outputs.toInt());
        return layer;
    }
} ML;
