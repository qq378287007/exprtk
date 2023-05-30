#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <algorithm>
using namespace std;

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
    inline set<string> relyUp() const { return m_RelyUp; }
    inline set<string> repyDown() const { return m_RelyDown; }
    inline bool needUpdate() const { return m_NeedUpdate; }

    inline void setExpressionStr(const string &expressionStr) { m_ExpressionStr = expressionStr; }
    inline void setValue(double value) { m_Value = value; }
    inline void setRelyUp(const set<string> &RelyUp) { m_RelyUp = RelyUp; }
    inline void setNeedUpdate(bool NeedUpdate) { m_NeedUpdate = NeedUpdate; }
    inline void addRelyUp(const string &name) { m_RelyUp.insert(name); }
    inline void addRelyDown(const string &name) { m_RelyDown.insert(name); }

private:
    string m_VarName;
    string m_ExpressionStr;

    double m_Value{0.0};
    set<string> m_RelyUp;
    set<string> m_RelyDown;

    bool m_NeedUpdate{false};
};

class VarManager
{
public:
    bool add(const string &name, const string &StrExp)
    {
        // 变量存在
        if (m_Var.count(name) == 1)
            return false;

        // 字符串表达式求值
        bool ok{false};
        double value = eval(StrExp, &ok);
        if (ok == false)
            return false;

        shared_ptr<Expression> &cur_exp{m_Var[name]};
        cur_exp.reset(new Expression(name, StrExp));
        cur_exp->setValue(value);

        // 获取依赖变量名
        const set<string> &RelyDown{getRelyDown(StrExp)};
        for (auto iter : RelyDown)
        {
            const shared_ptr<Expression> &exp{m_Var.at(iter)};
            exp->addRelyUp(name);
            cur_exp->addRelyDown(iter);
        }

        return true;
    }

    void remove(const string &name)
    {
        const set<string> &names{getRelyUp(name)};
        for (auto str : names)
            m_Var.erase(str);
        m_Var.erase(name);

        set<string> keys;
        for (auto it : m_Var)
            keys.insert(it.first);

        for (auto iter : m_Var)
        {
            const shared_ptr<Expression> &exp{iter.second};
            const set<string> &RelyUp{exp->relyUp()};
            set<string> tmp;
            set_intersection(RelyUp.begin(), RelyUp.end(), keys.begin(), keys.end(), inserter(tmp, tmp.begin())); /*取交集运算*/
            exp->setRelyUp(tmp);
        }
    }

    shared_ptr<Expression> check(const string &name)
    {
        return m_Var[name];
    }

    bool update(const string &name, const string &StrExp)
    {
        // 变量不存在
        if (m_Var.count(name) == 0)
            return false;

        // 依赖变量不存在
        const set<string> &RelyUp{getRelyUp(name)};
        const set<string> &RelyDown{getRelyDown(StrExp)};
        for (auto iter : RelyDown)
        {
            if (iter == name || m_Var.count(iter) == 0 || RelyUp.count(iter) == 1)
                return false;
        }

        // 字符串表达式求值
        bool ok{false};
        double value = eval(StrExp, &ok);
        if (ok == false)
            return false;

        const shared_ptr<Expression> &new_exp{m_Var.at(name)};
        new_exp->setExpressionStr(StrExp);
        new_exp->setValue(value);
        new_exp->setNeedUpdate(false);
        for (auto iter : RelyDown)
        {
            const shared_ptr<Expression> &exp{m_Var.at(iter)};
            exp->addRelyUp(name);
            new_exp->addRelyDown(iter);
        }

        // TODO:
        updateUp(name);
        return true;
    }

    void print()
    {
        for (auto iter : m_Var)
            cout << "name:" << iter.second->varName() << ", value:" << iter.second->value() << ", str:" << iter.second->expressionStr() << endl;
    }

    // 字符串表达式求值
    double eval(const string &StrExp, bool *ok = nullptr)
    {
        if (ok != nullptr)
            *ok = false;
        const set<string> &RelyDown{getRelyDown(StrExp)};
        for (auto iter : RelyDown)
        {
            // 依赖变量不存在
            if (m_Var.count(iter) == 0)
                return 0.0;
        }

        if (ok != nullptr)
            *ok = true;
        return 0.0;
    }

    // private:
    set<string> getRelyUp(const string &name)
    {
        set<string> RelyUp;

        const shared_ptr<Expression> &exp{m_Var.at(name)};
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

    // TODO:
    void updateUp(const string &name)
    {
        set<string> m_RelyUp{getRelyUp(name)};
        for (auto iter : m_RelyUp)
            m_Var.at(iter)->setNeedUpdate(true);

        while (!m_RelyUp.empty())
        {
            for (auto iter : m_RelyUp)
            {
                if (canUpdate(iter))
                {
                    const shared_ptr<Expression> &exp{m_Var.at(iter)};
                    double value = eval(exp->expressionStr());
                    exp->setValue(value);
                    exp->setNeedUpdate(false);
                    m_RelyUp.erase(iter);
                    break;
                }
            }
        }
    }

    bool canUpdate(const string &name)
    {
        const shared_ptr<Expression> &exp{m_Var.at(name)};
        const set<string> &RelyDown{exp->repyDown()};
        for (auto iter : RelyDown)
        {
            const shared_ptr<Expression> &tmp_exp{m_Var.at(iter)};
            if (tmp_exp->needUpdate() == true)
                return false;
        }
        return true;
    }

    // 获取字符串表达式依赖的变量名
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
                string SubStr = StrExp.substr(pos, n);
                RelyDown.insert(SubStr);
            }
        }

        static vector<string> strs{"pi", "t", "_", "sin", "cos", "tan"};
        for (auto s : strs)
            RelyDown.erase(s);

        return RelyDown;
    }

private:
    map<string, shared_ptr<Expression>> m_Var;
};

int main()
{
    VarManager vars;
    cout << "****add****" << endl;
    vars.add("a", "1+2+3");
    vars.add("b", "1+2+3*a");
    vars.add("c", "1+2+3*a+b");
    vars.add("d", "1+2+3+b");
    vars.add("e", "1+2+3*c+b");
    vars.add("f", "1+2+3+b-a+c*d");
    vars.print();
    cout << "****add****" << endl;

    cout << "****remove****" << endl;
    vars.remove("c");
    vars.print();
    cout << "****remove****" << endl;

    cout << "****update****" << endl;
    cout << vars.update("g", "") << endl;
    cout << vars.update("c", "x+y") << endl;
    cout << vars.update("a", "2*8") << endl;
    vars.print();
    cout << "****update****" << endl;

    cout << "Over!\n";
    return 0;
}