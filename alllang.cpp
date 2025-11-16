#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <variant>
using json = nlohmann::json;

//////////////////////////////////////////////////////////////////////
// 1) 언어 스펙 입력 (키워드/문법/패턴 외부 JSON으로 로딩)
//////////////////////////////////////////////////////////////////////

struct LanguageSpec {
    std::vector<std::string> keywords;
    std::string assignment;   // 예: "="
    std::string call_syntax;  // 예: "call"
    std::string func_syntax;  // 예: "func"
};

LanguageSpec loadSpec(const std::string& filename) {
    std::ifstream f(filename);
    json j; f >> j;

    LanguageSpec spec;
    spec.keywords   = j["keywords"].get<std::vector<std::string>>();
    spec.assignment = j["assignment"];
    spec.call_syntax= j["call"];
    spec.func_syntax= j["function"];

    return spec;
}

//////////////////////////////////////////////////////////////////////
// 2) 토크나이저 (언어 스펙 기반)
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
// 3) AST (고정 구조지만 문법 요소는 외부에서 결정)
//////////////////////////////////////////////////////////////////////

struct VarNode { std::string name; std::string expr; };
struct CallNode{ std::string name; std::vector<std::string> args; };
struct FuncNode{ std::string name; std::vector<std::variant<VarNode,CallNode>> body; };

//////////////////////////////////////////////////////////////////////
// 4) 파서: 동적 키워드 기반
//////////////////////////////////////////////////////////////////////

FuncNode parse(const std::vector<std::string>& t, const LanguageSpec& spec) {
    FuncNode fn;
    fn.name = t[1]; // fn / def / func 여부와 상관 없음 (입력 스펙에서 온 것)

    // 본문 탐색
    size_t start=0, end=t.size();
    for (size_t i=0;i<t.size();i++)
        if (t[i]=="{") start=i+1;

    for (size_t i=start;i<t.size();i++)
        if (t[i]=="}") end=i;

    // 동적 키워드 기반 파싱
    for (size_t i=start;i<end;i++) {

        // 변수 선언 키워드: spec에서 가져옴
        for (auto& kw : spec.keywords) {
            if (t[i]==kw && kw=="var") {
                VarNode v;
                v.name = t[i+1];
                v.expr = t[i+3];
                fn.body.push_back(v);
                i+=3;
            }
        }

        // 함수 호출 키워드도 spec.call_syntax 로 받음
        if (t[i]==spec.call_syntax) {
            CallNode c;
            c.name = t[i+1];
            c.args = { t[i+2] };
            fn.body.push_back(c);
            i+=2;
        }

        // print 같은 사용자 정의 호출도 JSON으로 받아 처리 가능
        if (t[i]=="print") {
            CallNode c;
            c.name="print";
            c.args={ t[i+2] };
            fn.body.push_back(c);
            i+=2;
        }
    }

    return fn;
}

//////////////////////////////////////////////////////////////////////
// 5) SpongeLang IR 생성 (고정)
//////////////////////////////////////////////////////////////////////

std::string to_ir(const FuncNode& f, const LanguageSpec& spec) {
    std::string ir;

    ir += spec.func_syntax + std::string(" ") + f.name + " {\n";

    for (auto& s : f.body) {
        std::visit([&](auto&& n){
            using T = std::decay_t<decltype(n)>;

            if constexpr (std::is_same_v<T,VarNode>) {
                ir += "    " + n.name + " " + spec.assignment + " " + n.expr + "\n";
            }
            else if constexpr (std::is_same_v<T,CallNode>) {
                ir += "    " + spec.call_syntax + " " + n.name;
                for (auto& a : n.args) ir += " " + a;
                ir += "\n";
            }

        }, s);
    }

    ir += "}\n";
    return ir;
}

//////////////////////////////////////////////////////////////////////
// 6) SpongeLang VM
//////////////////////////////////////////////////////////////////////

void execute(const std::string& ir) {
    std::unordered_map<std::string,int> vars;
    std::istringstream ss(ir);
    std::string w;

    while (ss >> w) {

        // 변수 = 값
        std::string eq;
        ss >> eq;
        if (eq == "=") {
            int v; ss >> v;
            vars[w]=v;
        }

        // call print x
        if (w=="call") {
            std::string name, arg;
            ss >> name >> arg;
            if (name=="print") std::cout<<vars[arg]<<"\n";
        }
    }
}

//////////////////////////////////////////////////////////////////////
// 7) MAIN – JSON으로 언어 스펙 받기
//////////////////////////////////////////////////////////////////////

int main(){
    std::cout << "Loading LanguageSpec...\n";

    LanguageSpec spec = loadSpec("langspec.json");

    std::cout << "Loaded keywords:\n";
    for(auto& k:spec.keywords) std::cout<<" - "<<k<<"\n";

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
