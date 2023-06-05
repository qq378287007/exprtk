#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <ctime>
using namespace std;

#include "exprtk.hpp"

class Expression
{
public:
    explicit Expression(const string &VarName, const string &ExpressionStr)
        : m_VarName(VarName), m_ExpressionStr(ExpressionStr)
    {
    }

    inline string varName() const { return m_VarName; }
    inline string expressionStr() const { return m_ExpressionStr; }

    inline double &value() { return m_Value; }
    inline set<string> & relyUp()  { return m_RelyUp; }
    inline const set<string> & repyDown() const { return m_RelyDown; }
    inline bool needUpdate() const { return m_NeedUpdate; }

    inline void setExpressionStr(const string &expressionStr) { m_ExpressionStr = expressionStr; }
    inline void setValue(double value) { m_Value = value; }
    inline void setNeedUpdate(bool NeedUpdate) { m_NeedUpdate = NeedUpdate; }

    inline void addRelyUp(const string &name) { m_RelyUp.insert(name); }
    inline void addRelyDown(const string &name) { m_RelyDown.insert(name); }

private:
    string m_VarName;//变量名
    string m_ExpressionStr;//表达式

    double m_Value{0.0};//变量值
    set<string> m_RelyUp;//向上依赖
    set<string> m_RelyDown;//向下依赖

    bool m_NeedUpdate{false};//是否需要更新
};

class VarManager
{
    enum Status
    {
        Success,//成功
        VariableExist,//变量存在
        VariableNotExist,//变量不存在
        SelfDependency,//自我依赖
        CircularDependency,//循环依赖
        DependencyNotExit,//依赖不存在
        ExpressionError,//表达式错误
        SystemVariable,//系统变量
    };

public:
    const string &status() const
    {
        static map<Status, string> m_Map{
            {Success, "Success"},
            {VariableExist, "Current variable exists"},
            {VariableNotExist, "Current variable does not exist"},
            {SelfDependency, "Self dependency"},
            {CircularDependency, "Circular dependency"},
            {DependencyNotExit, "Dependency does not exist"},
            {ExpressionError, "Expression error"},
            {SystemVariable, "System variable"},
        };
        return m_Map[m_Status];
    }

    VarManager() {
        expression.register_symbol_table(symbol_table);

        m_SystemVar["pi"].reset(new Expression("pi", "3.1415926"));
        for (auto iter : m_SystemVar)
        {
            const shared_ptr<Expression>& exp{ iter.second };
            parser.compile(exp->expressionStr(), expression);
            double value = expression.value();
            exp->setValue(value);
            symbol_table.add_variable(exp->varName(), exp->value()); // 添加变量名和变量值
        }
    }
public:
    bool add(const string &name, const string &StrExp)
    {
        if (m_SystemVar.count(name) == 1)// 系统变量
        {
            m_Status = SystemVariable;
            return false;
        }

        if (m_CustomVar.count(name) == 1) // 变量存在
        {
            m_Status = VariableExist;
            return false;
        }

        
        bool ok{false};
        double value = eval(StrExp, &ok);// 字符串解析
        if (ok == false)
            return false;

        shared_ptr<Expression> &cur_exp{m_CustomVar[name]};
        cur_exp.reset(new Expression(name, StrExp));
        cur_exp->setValue(value);

        symbol_table.add_variable(name, cur_exp->value());  // 添加变量名和变量值

        // 添加上下依赖
        const set<string> &RelyDown{getRelyDown(StrExp)};
        for (auto iter : RelyDown)
        {
            const shared_ptr<Expression> &exp{m_CustomVar.at(iter)};
            exp->addRelyUp(name);
            cur_exp->addRelyDown(iter);
        }

        return true;
    }

    void remove(const string &name)
    {
        // 移除父依赖数据
        const set<string> &names{getRelyUp(name)};
        for (auto str : names){
            m_CustomVar.erase(str);
            symbol_table.remove_variable(str);
        }

        m_CustomVar.erase(name);
        symbol_table.remove_variable(name);
 
        // 获取所有自定义变量名
        set<string> keys;
        for (auto it : m_CustomVar)
            keys.insert(it.first);
        
        for (auto iter : m_CustomVar)
        {
            const shared_ptr<Expression>& exp{ iter.second };

            //遍历并删除不存在的父依赖
            set<string>& RelyUp{ exp->relyUp() };
            for (auto iter_RelyUp = RelyUp.begin(); iter_RelyUp != RelyUp.end(); ) {

                if (keys.count(*iter_RelyUp) == 0)
                    iter_RelyUp = RelyUp.erase(iter_RelyUp);
                else
                    iter_RelyUp++;
            }
        }
    }

    bool update(const string &name, const string &StrExp)
    {
        // 系统变量
        if (m_SystemVar.count(name) == 1)
        {
            m_Status = SystemVariable;
            return false;
        }

        // 变量不存在
        if (m_CustomVar.count(name) == 0){
            m_Status = VariableNotExist;
            return false;
        }

        // 依赖合理性
        const set<string> &RelyUp{getRelyUp(name)};
        const set<string> &RelyDown{getRelyDown(StrExp)};
        for (auto iter : RelyDown)
        {
            // 自身依赖 || 依赖不存在 || 循环依赖
            if (iter == name){
                m_Status = SelfDependency;
                return false;
            }
            if (m_CustomVar.count(iter) == 0){
                m_Status = DependencyNotExit;
                return false;
            }
            if (RelyUp.count(iter) == 1){
                m_Status = CircularDependency;
                return false;
            }
                return false;
        }

        // 字符串解析
        bool ok{false};
        double value = eval(StrExp, &ok);
        if (ok == false)
            return false;

        // 表达式、值更新
        const shared_ptr<Expression> &new_exp{m_CustomVar.at(name)};
        new_exp->setExpressionStr(StrExp);
        new_exp->setValue(value);

        // 依赖更新
        for (auto iter : RelyDown)
        {
            const shared_ptr<Expression> &exp{m_CustomVar.at(iter)};
            exp->addRelyUp(name);
            new_exp->addRelyDown(iter);
        }

        // 父依赖数值更新
        updateUp(name);
        return true;
    }

    void print()
    {
        for (auto iter : m_CustomVar)
            cout << "name:" << iter.second->varName() << ", value:" << iter.second->value() << ", str:" << iter.second->expressionStr() << endl;
    }

    // 字符串解析
    double eval(const string &StrExp, bool *ok = nullptr)
    {
        if (ok != nullptr)
            *ok = false;

        const set<string> &RelyDown{getRelyDown(StrExp)};
        for (auto iter : RelyDown)
        {
            if (m_CustomVar.count(iter) == 0){// 依赖变量不存在
                m_Status = VariableNotExist;
                return 0.0;
            }
        }

        bool flag = parser.compile(StrExp, expression);// 表达式求值
        double value = expression.value();
        if(flag){
            m_Status = Success;
        }else{
            m_Status = ExpressionError;
        }

        if (ok != nullptr)
            *ok = flag;
        return value;
    }

private:
    // 递归获取父依赖
    set<string> getRelyUp(const string &name)
    {
        set<string> RelyUp;

        const shared_ptr<Expression> &exp{m_CustomVar.at(name)};
        const set<string> &m_RelyUp{exp->relyUp()};
        for (auto iter : m_RelyUp)
        {
            const set<string> &tmp{getRelyUp(iter)};
            for (auto tmp_iter : tmp)
                RelyUp.insert(tmp_iter);
            RelyUp.insert(iter);
        }
        return RelyUp;
    }

    // 更新所有父依赖的数值
    void updateUp(const string &name)
    {
        set<string> m_RelyUp{getRelyUp(name)};
        for (auto iter : m_RelyUp)
            m_CustomVar.at(iter)->setNeedUpdate(true);

        while (!m_RelyUp.empty())
        {
            for (auto iter : m_RelyUp)
            {
                if (canUpdate(iter))
                {
                    const shared_ptr<Expression> &exp{m_CustomVar.at(iter)};
                    double value = eval(exp->expressionStr());
                    exp->setValue(value);
                    exp->setNeedUpdate(false);
                    m_RelyUp.erase(iter);
                    break;
                }
            }
        }
    }

    // 所有子依赖有数值时，可以求解
    bool canUpdate(const string &name)
    {
        const shared_ptr<Expression> &exp{m_CustomVar.at(name)};
        const set<string> &RelyDown{exp->repyDown()};
        for (auto iter : RelyDown)
        {
            const shared_ptr<Expression> &tmp_exp{m_CustomVar.at(iter)};
            if (tmp_exp->needUpdate() == true) //子依赖需要更新时，当前变量的表达式无法求解
                return false;
        }
        return true;
    }

    // 字符串解析获取子依赖
    set<string> getRelyDown(const string &StrExp)
    {
        set<string> RelyDown;

        size_t len = StrExp.size();
        for (size_t i{0}; i < len; i++)
        {
            if (StrExp[i] == '_' || isalpha(StrExp[i]))
            {
                size_t pos = i;
                while ((StrExp[i] == '_' || isalpha(StrExp[i]) || isdigit(StrExp[i])) && i < len)
                    i++;
                size_t n = i - pos;
                string SubStr = StrExp.substr(pos, n);//提取变量名
                RelyDown.insert(SubStr);
            }
        }

        for (auto s : m_SystemVar)
            RelyDown.erase(s.first);//移除系统变量

        static vector<string> strs{"_", "sin", "cos", "tan"};
        for (auto s : strs)
            RelyDown.erase(s);//移除数学函数名

        return RelyDown;
    }

private:
    map<string, shared_ptr<Expression>> m_CustomVar;//自定义变量
    map<string, shared_ptr<Expression>> m_SystemVar;//系统变量
    Status m_Status{Success};//状态

    exprtk::symbol_table<double> symbol_table;
    exprtk::expression<double> expression;
    exprtk::parser<double> parser;
};

int main()
{
    clock_t start;
    clock_t end;

    VarManager vars;

    start = clock();
    cout << "****add****" << endl;
    vars.add("a", "1+2+3");
    vars.add("b", "1+2+3*a");
    vars.add("c", "1+2+3*a+b");
    vars.add("d", "1+2+3.25+cos(b)");
    vars.add("e", "1+2+3*c+b");
    vars.add("f", "1+2+3+b-a+9+(-c*d)");
    for (int i = 0; i < 9999; i++) {
        vars.add("var" + to_string(i), to_string(i) + " + a");
    }
    //vars.print();
    cout << "****add****" << endl;
    end = clock();
    cout << "add time: " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;
    cout << "\n\n";

    start = clock();
    cout << "****remove****" << endl;
    vars.remove("c");
    //vars.print();
    cout << "****remove****" << endl;
    end = clock();
    cout << "remove time: " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;
    cout << "\n\n";

    start = clock();
    cout << "****update****" << endl;
    vars.update("a", "2*8");
    //vars.print();
    cout << "****update****" << endl;
    end = clock();
    cout << "update time: " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;
    cout << "\n\n";

    start = clock();
    cout << "****eval****" << endl;
    for (int i = 0; i < 9999; i++) {
        string str{ "sin(cos(var" + to_string(i) + " - a) + b) - d*a " };
        double value = vars.eval(str);
        //cout << str << " = " << value << endl;
    }
    cout << "****eval****" << endl;
    end = clock();
    cout << "eval time: " << (double)(end - start) / CLOCKS_PER_SEC <<"s"<< endl;
    cout << "\n\n";

    cout << "Over!\n";

    cin.get();
    cin.get();
    cin.get();
    cin.get();
    cin.get();
    cin.get();
    cin.get();
    cin.get();
    cin.get();
    return 0;
}
