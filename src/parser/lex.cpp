#include<iostream>
#include <QTextStream>
#include "lex.h"
#include <QRegExp>

Token::Token(TokenType type, QString text)
{
    this->m_text = text;
    this->m_type = type;
}

Token::Token(TokenType type, QString text, int nestingLevel)
{
    this->m_text = text;
    this->m_type = type;
    this->nestingLevel = nestingLevel;
}


void LexicalAnalyser::sortByLength(QStringList & list)
{
    int n = list.count();
    QString temp;

    for(int i = 0; i < n; i++)
    {
        for(int j = 1; j < n; j++)
        {
            if (list[j].count() > list[j-1].count())
            {
                temp = list[j];
                list[j] = list[j-1];
                list[j-1] = temp;
            }
        }
    }
}

void LexicalAnalyser::printTokens()
{
    QTextStream qout(stdout);
    foreach (Token token, m_tokens)
    {
        qout << token.toString() << endl;
    }
}

QList<Token> LexicalAnalyser::tokens()
{
    return this->m_tokens;
}

QString expression(QList<Token> const &symbol_que, int position = 0)
{
    int n = symbol_que.count();
    int nesting_level = symbol_que[position].nestingLevel;
    QString expression;
    for(int i = position; i < n; i++)
    {
        if (nesting_level < symbol_que[i].nestingLevel)
        {
            nesting_level++;
        }

        if (nesting_level > symbol_que[i].nestingLevel)
        {
            nesting_level--;
        }
    }
    return expression;
}


void LexicalAnalyser::setExpression(const QString &expr)
{
    QString exprTrimmed = expr.trimmed().replace(" ", "");

    QTextStream qout(stdout);
    QStringList operators;
    QStringList functions;
    QList<Terminals>  terminals;

    sortByLength(m_variables);
    terminals.append(Terminals(TokenType_VARIABLE, m_variables));

    operators << "(" << ")" << "+" << "**" << "-" << "*" << "/" << "^" << "==" << "&&" << "||" << "<=" << ">=" << "!=" << "<" << ">" << "=" << "?" << ":" << ",";
    sortByLength(operators);
    terminals.append(Terminals(TokenType_OPERATOR, operators));
    functions << "sin" << "cos" << "asin" <<  "acos" << "atan" << "sinh" << "cosh" <<
                 "tanh" << "asinh" << "acosh" << "atanh" << "log2" << "log10" << "log" <<
                 "exp" << "sqrt" << "sign" << "abs" << "min" << "max" << "sum" << "avg" << "pow";
    // TODO: conflict "tan" with "tanx", "tany", "tanr", "tanz" in surface forms
    sortByLength(functions);
    terminals.append(Terminals(TokenType_FUNCTION, functions));

    int pos = 0;

    QRegExp r_exp = QRegExp("[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");

    int loop_counter = 0;
    int n = exprTrimmed.count();
    int old_pos = 0;
    int nesting_level = 0;

    while ((pos < n) && (loop_counter < n))
    {
        loop_counter++;
        int index = r_exp.indexIn(exprTrimmed, pos);

        if(index == pos)
        {
            QString text = r_exp.capturedTexts().takeFirst();
            Token symbol(TokenType_NUMBER, text);
            symbol.nestingLevel = nesting_level;
            pos += text.count();
            m_tokens.append(symbol);
        }

        foreach(Terminals terminal_symbols, terminals)
        {
            terminal_symbols.find(exprTrimmed, m_tokens, pos, nesting_level);
        }

        if (old_pos == pos)
        {
            throw ParserException(QString("Unexpected symbol '%1' on position %2 in expression '%3'").arg(exprTrimmed[pos]).arg(pos).arg(expr),
                                  expr,
                                  pos,
                                  exprTrimmed.at(pos));
            break;
        }
        else
        {
            old_pos = pos;
        }
    }
}

//ToDo: Improve, it should be within Syntax analyzer
QString LexicalAnalyser::replaceOperatorByFunction(QString expression)
{
    // Reguler expression for ^any_number
    QRegExp re("(\\^)([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?)");
    QString expr = expression;
    int position = 0;
    bool isReplaced = false;
    while (position != -1)
    {
        position = re.indexIn(expr);
        if (position != -1)
        {
            isReplaced = true;
            QString replace = "";
            QString exponent = re.capturedTexts()[2];
            //std::cout << position << " " << exponent.toStdString() << std::endl;
            if (expr[position-1] == ')')
            {
                int j = position - 1;
                int level = 0;
                do
                {
                    if (expr[j] == '(' )
                        level = level - 1;
                    if (expr[j] == ')' )
                        level = level + 1;
                    j = j - 1;
                } while (level != 0);

                for (int k = 0; k <= j; k++ )
                {
                    replace += expr[k];
                }
                replace += "pow";
                for (int k = j+1; k < position - 1; k++ )
                {
                    replace += expr[k];
                }
                replace += ", " + exponent + ")";
                for (int k = position + 1 + exponent.length(); k < expr.length(); k++)
                {
                    replace += expr[k];
                }
            }            
            expr = replace;
        }
    }

    if (isReplaced == true)
        return expr;
    else
        return expression;
}


Terminals::Terminals(TokenType terminal_type, QStringList terminal_list)
{

    int n = terminal_list.count();
    for(int i = 0; i < n; i++)
    {
        Token symbol = Token(terminal_type, terminal_list[i]);
        this->m_list.append(symbol);
    }
}

void Terminals::find(const QString &s, QList<Token> &symbol_que, int &pos, int &nesting_level)
{
    Token symbol;
    int n = this->m_list.count();
    for (int i = 0; i < n; i++)
    {
        int loc_pos = s.indexOf(m_list[i].toString(), pos);
        if (loc_pos == pos) {
            symbol = Token(m_list[i].type(), m_list[i].toString());

            if (symbol.toString() == "(")
            {
                symbol.nestingLevel = nesting_level++;
            } else
                if (symbol.toString() == ")")
                {
                    symbol.nestingLevel = --nesting_level;
                }
                else
                    symbol.nestingLevel = nesting_level;

            pos += m_list[i].toString().count();
            symbol_que.append(symbol);
            break;
        }
    }
}

void Terminals::print()
{
    QTextStream qout(stdout);
    int n =this->m_list.count();
    for(int i = 0; i < n; i++)
    {
        qout << this->m_list[i].toString() << endl;
    }
}