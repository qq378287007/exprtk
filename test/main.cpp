#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
using namespace std;

#include "exprtk.hpp"

//获取表达式str中所有依赖变量
set<string> getRely(const string& str) {
	set<string> rely;
	int n = str.size();

	for (int i = 0; i < n; i++) {
		if (str[i] == '_' || isalpha(str[i])) {
			int start = i;
			while ((str[i] == '_' || isalpha(str[i]) || isdigit(str[i])) && i<n)
				i++;
			int end = i;
			string sub = str.substr(start, end - start);
			rely.insert(sub);
		}
	}

	static vector<string> strs{"pi", "t", "_", "sin", "cos", "tan"};
	for (auto s : strs)
		rely.erase(s);
	
	return rely;
}

//数值字符串解析
//eval("1+2*3-4/5+sin(6.2)+cos(3.14)")
double eval(const string& str, bool *ok = nullptr) {
	exprtk::expression<double> expression;
	exprtk::parser<double> parser;
	 *ok = parser.compile(str, expression);
	return expression.value();
}

class Expression {
public:
	Expression(const string &VarName, const string &ExpressionStr)
		:m_VarName(VarName)
		,m_ExpressionStr(ExpressionStr) 
	{}

	inline string varName() const { return m_VarName; }
	inline string expressionStr() const { return m_ExpressionStr; }

	inline double &value()  { return m_Value; }
	inline set<string> repyUp() const { return m_RelyUp; }
	inline set<string> repyDown() const { return m_RelyDown; }
	inline bool needUpdate() const { return m_NeedUpdate; }

	inline void setValue(double value) { m_Value = value; }
	inline void setRepyDown(set<string> RepyDown) { m_RelyDown = RepyDown; }
	inline void addRelyUp(const string& name) { m_RelyUp.insert(name); }
	inline void setNeedUpdate(bool NeedUpdate) { m_NeedUpdate = NeedUpdate; }


private:
	string m_VarName;
	string m_ExpressionStr;

	double m_Value{0.0};
	set<string> m_RelyUp;
	set<string> m_RelyDown;

	bool m_NeedUpdate{false};
};

void evalList(vector<Expression> &vars) {
	exprtk::expression<double>  expression;
	exprtk::parser<double> parser;
	exprtk::symbol_table<double>  symbol_table;
	//symbol_table.add_constants();

	expression.register_symbol_table(symbol_table);

    int n = vars.size();
    for (int i = 0; i < n; i++) {
		if (i > 0) {
			Expression& last_exp = vars.at(i-1);
			symbol_table.add_variable(last_exp.varName(), last_exp.value());
		}

		Expression& exp = vars[i];
		parser.compile(exp.expressionStr(), expression);
		double value = expression.value();
		exp.setValue(value);
	}
}

class AllVar {
public:
	bool add(const string& name, const string& str) {
		//已包含此变量
		if(data.count(name))
			return false;

		//获取所有依赖变量
		const set<string>& m_RelyDown = getRely(str);
		for (auto iter = m_RelyDown.cbegin(); iter != m_RelyDown.cend(); iter++) {
			//依赖变量不存在
			if (data.count(*iter) == 0)
				return false;
		}

		exprtk::symbol_table<double>  symbol_table;
		exprtk::expression<double>  expression;
		exprtk::parser<double> parser;

		for (auto iter = m_RelyDown.cbegin(); iter != m_RelyDown.cend(); iter++) {
			shared_ptr<Expression>& exp = data[*iter];
			symbol_table.add_variable(exp->varName(), exp->value());
		}
		expression.register_symbol_table(symbol_table);


		//表达式错误
		if (!parser.compile(str, expression))
			return false;

		double value = expression.value();

		shared_ptr<Expression>& new_exp = data[name];
		new_exp.reset(new Expression(name, str));

		new_exp->setValue(value);
		new_exp->setRepyDown(m_RelyDown);


		for (auto iter = m_RelyDown.cbegin(); iter != m_RelyDown.cend(); iter++) {
			const shared_ptr<Expression>& exp = data[*iter];
			exp->addRelyUp(name);
		}

		return false;
	}

	
	void remove(const string& name) {
		set<string> names{ replyUp(name) };
		names.insert(name);
		for (auto str : names)
			data.erase(str);
	}

	shared_ptr<Expression> check(const string& name) {
		return data[name];
	}

	bool update(const string& name, const string& str) {
		//不包含此变量
		if (data.count(name) == 0)
			return false;


		set<string> m_RelyUp{replyUp(name)};
		//获取所有依赖变量
		const set<string>& m_RelyDown = getRely(str);
		for (auto iter = m_RelyDown.cbegin(); iter != m_RelyDown.cend(); iter++) {
			//依赖变量不存在
			if (data.count(*iter) == 0 || m_RelyUp.count(*iter) == 1)
				return false;
		}



		exprtk::symbol_table<double>  symbol_table;
		exprtk::expression<double>  expression;
		exprtk::parser<double> parser;

		for (auto iter = m_RelyDown.cbegin(); iter != m_RelyDown.cend(); iter++) {
			shared_ptr<Expression>& exp = data[*iter];
			symbol_table.add_variable(exp->varName(), exp->value());
		}
		expression.register_symbol_table(symbol_table);


		//表达式错误
		if (!parser.compile(str, expression))
			return false;

		double value = expression.value();

		const shared_ptr<Expression>& new_exp = data.at(name);
		new_exp->setValue(value);
		new_exp->setRepyDown(m_RelyDown);
		new_exp->setNeedUpdate(false);

		for (auto iter = m_RelyDown.cbegin(); iter != m_RelyDown.cend(); iter++) {
			const shared_ptr<Expression>& exp = data[*iter];
			exp->addRelyUp(name);
		}


		//TODO:
		updateUp(name);


		return true;
	}



	void print() {
		for (auto iter : data) 
			cout << iter.second->varName() << " = " << iter.second->value() << endl;
	}

private:
	set<string> replyUp(const string &name) {
		set<string> reply;

		const shared_ptr<Expression>& exp = data[name];
		const set<string> &strs = exp->repyUp();
		while (!strs.empty()) {
			for (auto iter = strs.cbegin(); iter != strs.cend(); iter++) {
				const set<string> tmp{replyUp(*iter) };
				for (auto tmp_iter = tmp.cbegin(); tmp_iter != tmp.cend(); tmp_iter++)
					reply.insert(*tmp_iter);
			}
		}
		return reply;
	}

	//TODO:
	void updateUp(const string& name) {

		set<string> m_RelyUp{ replyUp(name) };
		for (auto iter = m_RelyUp.cbegin(); iter != m_RelyUp.cend(); iter++)
			data.at(*iter)->setNeedUpdate(true);

		while (!m_RelyUp.empty()) {
			for (auto iter = m_RelyUp.cbegin(); iter != m_RelyUp.cend(); iter++) {
                if (canUpdate(*iter)) {
					eval(*iter);
					m_RelyUp.erase(iter);
					break;
				}
			}
		}
	}

	void eval(const string& name) {
		exprtk::symbol_table<double>  symbol_table;
		exprtk::expression<double>  expression;
		exprtk::parser<double> parser;

		const shared_ptr<Expression>& exp = data.at(name);
		const set<string>& m_RelyDown = exp->repyDown();
		for (auto iter = m_RelyDown.cbegin(); iter != m_RelyDown.cend(); iter++) {
			shared_ptr<Expression>& tmp_exp = data[*iter];
			symbol_table.add_variable(tmp_exp->varName(), tmp_exp->value());
		}
		expression.register_symbol_table(symbol_table);

		parser.compile(exp->expressionStr(), expression);

		double value = expression.value();

		exp->setValue(value);
		exp->setNeedUpdate(false);
	}

	bool canUpdate(const string& name) {
		const shared_ptr<Expression>& exp = data.at(name);
		const set<string>& m_RelyDown = exp->repyDown();
		for (auto iter = m_RelyDown.cbegin(); iter != m_RelyDown.cend(); iter++) {
			shared_ptr<Expression>& tmp_exp = data[*iter];
			if (tmp_exp->needUpdate())
				return false;
		}
		return true;
	}
private:
	map<string, shared_ptr<Expression>> data;
};

int main()
{
	//cout <<  "value: " << eval("sin(3.1415/2.0)") << endl;

/*
	{
		vector<Expression> vars{
			Expression("a", "sin(1.2)"),
			Expression("b", "1.3 + cos(a)"),
			Expression("c", "1.4 + b * b"),
		};
		evalList(vars);

		for (int i = 0; i < vars.size(); i++) {
			Expression& exp = vars.at(i);
			cout << exp.varName() << " = " << exp.value() << endl;
		}
	}
*/

	{
		AllVar vars;
		vars.add("a", "1+2+3");
		vars.add("b", "1+2+3*a");
		vars.add("c", "1+2+3*a+b");
		//vars.update("b", "20");
		vars.print();

	}

	cout << "Over!\n";
	return 0;
}
