#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include "json.hpp"   // ← 수정됨
#include <variant>
using json = nlohmann::json;

//////////////////////////////////////////////////////////////////////
// 1) LanguageSpec
//////////////////////////////////////////////////////////////////////

struct LanguageSpec {
    std::vector<std::string> keywords;
    std::string assignment;
    std::string call_syntax;
    std::string func_syntax;
};

LanguageSpec loadSpec(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        std::cerr << "ERROR: Cannot open " << filename << "\n";
        exit(1);
    }

    json j; 
    f >> j;

    LanguageSpec spec;
    spec.keywords    = j["keywords"].get<std::vector<std::string>>();
    spec.assignment  = j["assignment"].get<std::string>();
    spec.call_syntax = j["call"].get<std::string>();
    spec.func_syntax = j["function"].get<std::string>();

    return spec;
}

//////////////////////////////////////////////////////////////////////
// 2) Tokenizer
//////////////////////////////////////////////////////////////////////

std::vector<std::string> tokenize(std::string_view src) {
    std::vector<std::string> t;
    std::string cur;

    for (char c: src) {
        if (isspace(c)) {
            if (!cur.empty()) { t.push_back(cur); cur.clear(); }
        }
        else if (strchr("{}():,+-*/=", c)) {
            if (!cur.empty()) { t.push_back(cur); cur.clear(); }
            t.emplace_back(1, c);
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) t.push_back(cur);

    return t;
}

//////////////////////////////////////////////////////////////////////
// 3) AST
//////////////////////////////////////////////////////////////////////

struct VarNode { std::string name; std::string expr; };
struct CallNode{ std::string name; std::vector<std::string> args; };
struct FuncNode{ std::string name; std::vector<std::variant<VarNode,CallNode>> body; };

//////////////////////////////////////////////////////////////////////
// 4) Parser (spec 기반)
//////////////////////////////////////////////////////////////////////

FuncNode parse(const std::vector<std::string>& t, const LanguageSpec& spec) {
    FuncNode fn;
    
    // func <name> ( ) {
    //             ^ index 1
    fn.name = t[1];

    size_t start=0, end=t.size();

    for (size_t i=0;i<t.size();i++)
        if (t[i]=="{") start=i+1;

    for (size_t i=0;i<t.size();i++)
        if (t[i]=="}") { end=i; break; }

    for (size_t i=start;i<end;i++) {

        // var x = 15
        if (t[i]=="var") {
            VarNode v;
            v.name = t[i+1];
            v.expr = t[i+3];
            fn.body.push_back(v);
            i+=3;
            continue;
        }

        // call print x
        if (t[i]==spec.call_syntax) {
            CallNode c;
            c.name = t[i+1];
            c.args = { t[i+2] };
            fn.body.push_back(c);
            i+=2;
            continue;
        }
    }

    return fn;
}

//////////////////////////////////////////////////////////////////////
// 5) To IR
//////////////////////////////////////////////////////////////////////

std::string to_ir(const FuncNode& f, const LanguageSpec& spec) {
    std::string ir;

    ir += spec.func_syntax + " " + f.name + " {\n";

    for (auto& node : f.body) {
        std::visit([&](auto&& n){
            using T = std::decay_t<decltype(n)>;

            if constexpr (std::is_same_v<T,VarNode>) {
                ir += "    " + n.name + " " + spec.assignment + " " + n.expr + "\n";
            }
            else if constexpr (std::is_same_v<T,CallNode>) {
                ir += "    " + spec.call_syntax + " " + n.name;
                for (auto& a: n.args) ir += " " + a;
                ir += "\n";
            }

        }, node);
    }

    ir += "}\n";
    return ir;
}

//////////////////////////////////////////////////////////////////////
// 6) Simple execution
//////////////////////////////////////////////////////////////////////

void execute(const std::string& ir) {
    std::unordered_map<std::string,int> vars;
    std::istringstream ss(ir);
    std::string w;

    while (ss >> w) {

        // var assignment
        std::string eq;
        if (!(ss >> eq)) break;

        if (eq == "=") {
            int v;
            ss >> v;
            vars[w]=v;
        }

        // call print x
        if (w=="call") {
            std::string name, arg;
            ss >> name >> arg;
            if (name=="print") {
                if (vars.count(arg))
                    std::cout << vars[arg] << "\n";
                else
                    std::cout << "(undefined) " << arg << "\n";
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
// 7) MAIN
//////////////////////////////////////////////////////////////////////

int main(){
    std::cout << "Loading LanguageSpec...\n";

    LanguageSpec spec = loadSpec("langspec.json");

    std::string src =
        "func main() { "
        "   var x = 15 "
        "   call print x "
        "}";

    auto tok = tokenize(src);
    auto ast = parse(tok,spec);
    auto ir  = to_ir(ast,spec);

    std::cout << "\n=== IR ===\n" << ir << "\n";
    std::cout << "\n=== EXECUTION ===\n";
    execute(ir);
}
