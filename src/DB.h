/**
 * @file        DB.h
 * @brief
 * @author      hjb
 * @version     1.0
 * @date        2023-11-02
 * @copyright   Copyright (c) 2023
 */

#pragma once

#include <iostream>
#include <format>
#include <string>
#include <vector>
#include "DDL.h"
#include "DML.h"
#include "DQL.h"

class DB {
private:
    std::string name = "db"; // 当前选中数据库
    std::string cmd = "";    // 用户指令
    std::string tmp = "";    // 用户上一条指令
    int flag = 0;            // 循环控制
    int cmd_end = 1;         // 语句结束判断
    int paren_end = 1;       // 括号匹配

public:
    DB() noexcept {
        this->name = "db";
    }
    DB(std::string database) noexcept {
        this->name = database;
    }
    ~DB() noexcept {
        clear();
    }

    void start() {
        // 主循环
        while (!flag) {
            update();
        }
    }
private:
    /*主循环*/
    void update() {
        if (paren_end == 0)
            cmd_end = 0;
        if (cmd_end == 1) // 正常显示
            std::cout << name << "> ";
        if (cmd_end == 0) {
            // 用户上一条指令未结束
            for (auto i = 0uz; i < name.size() - 1; ++i) {
                std::cout << " ";
            }
            std::cout << "-> ";
        }
        if (std::getline(std::cin, cmd)) {
            // 读取用户指令
            if (cmd.size() < 1)
                return;
            str_process(cmd); // 指令预处理
            sql_process(cmd);    // 语句解析
        }
    }
    
    /*语句处理*/
    void sql_process(std::string &cmd) {
        std::string s_test = tmp + " " + cmd;
        if (std::count(s_test.begin(), s_test.end(), '(') != std::count(s_test.begin(), s_test.end(), ')')) {
            paren_end = 0;
            cmd_end = 0;
        } else {
            paren_end = 1;
        }
        if (cmd.find(";") + 1 && paren_end == 0) {
            // 括号不匹配
            std::cout << "Syntax error!(Parens mismatch)" << std::endl;
            paren_end = 1;
            cmd_end = 1;
            tmp = "";

            return;
        }
        if (cmd.find(";") + 1 && paren_end == 1) {
            // 语句结束
            if (cmd_end == 1 && cmd.size() == 1) // 空语句
                return;
            cmd_end = 1;

            if (tmp.size() > 0)
                tmp = tmp + " ";

            cmd = tmp + cmd; // 合成新语句
            tmp = "";

            if (cmd == "exit;") {
                // 退出指令
                flag = 1;
            } else if (cmd.size() > 0) {
                // 语句解析
                cmd.pop_back(); // 去除末尾分号
                str_process(cmd);

                std::vector<std::string> res;
                str_split(cmd, res, " "); // 拆解语句
                sql_execute(res, cmd);       // 语句执行
            } else {
                // 无法解析
                std::cout << "Syntax error!" << std::endl;
                paren_end = 1;
                cmd_end = 1;
                tmp = "";

                return;
            }
        } else {
            // 语句未结束
            cmd_end = 0;
            if (tmp.size() > 0)
                tmp = tmp + " ";

            tmp = tmp + cmd;

            if (tmp == "exit") {
                flag = 1;
            }
        }
    }
    /*sql语句执行*/
    bool sql_execute(const std::vector<std::string> &res, const std::string &cmd) {
        bool status = true;
        // create xxx xxx
        {
            // create database
            std::string create_database_xxx = "create\\sdatabase\\s[a-zA-Z]+[a-zA-Z0-9]*";
            std::string create_database_regex = "^\\s?" + create_database_xxx + "\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd, std::regex(create_database_regex))) {
                // 创建数据库
                if (DDL::createDatabase(res[2])) {
                    std::cout << "Create database successfully!" << std::endl;
                } else {
                    status &= false;
                }
                return status;
            }
        }
        {
            // create table
            std::string create_table_xxx = "create\\stable\\s[a-zA-Z]+[a-zA-Z0-9]*";
            std::string name_type_constraint = "[a-zA-Z]+[a-zA-Z0-9]*\\s(int|string)\\s?(\\sprimary)?";
            std::string create_table_regex = "^\\s?" + create_table_xxx + "\\s?\\(\\s?(" +
                    name_type_constraint + "\\s?,\\s?" + ")*(" + name_type_constraint + "\\s?,?\\s?)" + "\\)\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd.c_str(), std::regex(create_table_regex))) {
                // 获取表名
                std::string table_name = res[2];
                if (table_name.find("(") + 1) {
                    while (table_name.back() != '(') {
                        table_name.pop_back();
                    }
                    table_name.pop_back();
                }
                // 创建表
                if (DDL::createTable(name, table_name, cmd)) {
                    std::cout << "Create table successfully!" << std::endl;
                } else {
                    status &= false;
                }
                return status;
            }
        }

        // insert xxx xxx
        {
            // insert xxx values (xxx)
            std::string insert_table_xxx = "insert\\s[a-zA-Z]+[a-zA-Z0-9]*\\svalues";
            std::string tuple_content = "(-?\\d+|\".*\")";
            std::string insert_table_regex = "^\\s?" + insert_table_xxx + "\\s?\\(\\s?(" +
                    tuple_content + "\\s?,\\s?)*" + tuple_content + "\\s?,?" + "\\s?\\)\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd, std::regex(insert_table_regex))) {
                // 获取表名
                std::string table_name = res[1];
                // 插入表数据
                if (DML::insertRecord(name, table_name, cmd))
                    std::cout << "Insert table successfully!" << std::endl;
                else
                    status &= false;

                return status;
            }
        }

        // use xxx
        {
            // use xxx
            std::string use_database_xxx = "use\\s[a-zA-Z]+[a-zA-Z0-9]*";
            std::string use_database_regex = "^\\s?" + use_database_xxx + "\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd, std::regex(use_database_regex))) {
                // 数据库存在判定
                if (searchDatabase(res[1])) {
                    // 切换所选中数据库
                    name = res[1];
                    std::cout << "Change database successfully!" << std::endl;
                } else {
                    status &= false;
                }
                return status;
            }
        }

        // select xxx from xxx [where xxx = xxx]
        {
            // select xxx from xxx
            std::string property_name = "([a-zA-Z]+[a-zA-Z0-9]*\\s?,\\s?)*[a-zA-Z]+[a-zA-Z0-9]*";
            std::string simple_condition = "([a-zA-Z]+[a-zA-Z0-9]*\\s?=\\s?((-?\\d+)|(\".*\")))";
            std::string paren_condition = std::format("(\\(\\s?{}\\s?\\))", simple_condition);
            std::string single_condition = std::format("({0}|{1})", simple_condition, paren_condition);
            std::string mutiply_condition = std::format("({0}|({0}\\s?,\\s?)*{0}|\\(\\s?({0}\\s?,\\s?)*{0}\\s?\\))", single_condition);
            std::string where_xxx = std::format("(\\swhere\\s{})?", mutiply_condition);
            std::string select_xxx = std::format("select\\s({}|\\*)\\sfrom\\s[a-zA-Z]+[a-zA-Z0-9]*", property_name);
            std::string select_regex = "^\\s?" + select_xxx + where_xxx + "\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd, std::regex(select_regex))) {
                // 查询表记录
                if (DQL::selectRecord(name, res, cmd)) {
                    std::cout << "Select record successfully!" << std::endl;
                } else {
                    status &= false;
                }
                return status;
            }
        }

        // delete xxx [where xxx]
        {
            // delete xxx
            std::string delete_table = "delete\\s[a-zA-Z]+[a-zA-Z0-9]*";
            std::string simple_condition = "([a-zA-Z]+[a-zA-Z0-9]*\\s?=\\s?((-?\\d+)|(\".*\")))";
            std::string paren_condition = std::format("(\\(\\s?{}\\s?\\))", simple_condition);
            std::string single_condition = std::format("({0}|{1})", simple_condition, paren_condition);
            std::string mutiply_condition = std::format("({0}|({0}\\s?,\\s?)*{0}|\\(\\s?({0}\\s?,\\s?)*{0}\\s?\\))", single_condition);
            std::string where_xxx = std::format("(\\swhere\\s{})?", mutiply_condition);
            std::string delete_table_regex = "^\\s?" + delete_table + where_xxx + "\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd, std::regex(delete_table_regex))) {
                // 表存在判定
                if (searchTable(name, res[1])) {
                    // 删除表记录
                    if (DML::deleteRecord(name, res[1], cmd)) {
                        std::cout << "Delete record successfully!" << std::endl;
                    } else {
                        status &= false;
                    }
                } else {
                    status &= false;
                }
                return status;
            }
        }

        // update xxx set xxx [where xxx]
        {
            // update xxx set xxx
            std::string update_xxx = "update\\s[a-zA-Z]+[a-zA-Z0-9]*\\sset\\s";
            std::string simple_condition = "([a-zA-Z]+[a-zA-Z0-9]*\\s?=\\s?((-?\\d+)|(\".*\")))";
            std::string paren_condition = std::format("(\\(\\s?{}\\s?\\))", simple_condition);
            std::string single_condition = std::format("({0}|{1})", simple_condition, paren_condition);
            std::string mutiply_condition = std::format("({0}|({0}\\s?,\\s?)*{0}|\\(\\s?({0}\\s?,\\s?)*{0}\\s?\\))", single_condition);
            std::string where_xxx = std::format("(\\swhere\\s{})?", mutiply_condition);
            std::string update_regex = "^\\s?" + update_xxx + simple_condition + where_xxx + "\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd, std::regex(update_regex))) {
                // 表存在判定
                if (searchTable(name, res[1])) {
                    // 更新表记录
                    if (DML::updateRecord(name, res[1], cmd)) {
                        std::cout << "Update record successfully!" << std::endl;
                    } else {
                        status &= false;
                    }
                } else {
                    status &= false;
                }
                return status;
            }
        }

        // drop xxx xxx
        {
            // drop database xxx
            std::string drop_database_xxx = "drop\\sdatabase\\s[a-zA-Z]+[a-zA-Z0-9]*";
            std::string drop_database_regex = "^\\s?" + drop_database_xxx + "\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd, std::regex(drop_database_regex))) {
                // 数据库存在判定
                if (searchDatabase(res[2])) {
                    // 若删除当前操作数据库则切换
                    if (name == res[2]) {
                        name = "db";
                    }
                    // 删除数据库
                    if (DDL::dropDatabase(res[2])) {
                        std::cout << "Drop database successfully!" << std::endl;
                    } else {
                        status &= false;
                    }
                } else {
                    status &= false;
                }
                return status;
            }
        }
        {
            // drop table xxx
            std::string drop_table_xxx = "drop\\stable\\s[a-zA-Z]+[a-zA-Z0-9]*";
            std::string drop_table_regex = "^\\s?" + drop_table_xxx + "\\s?" + "$";
            // 正则表达式匹配
            if (std::regex_match(cmd, std::regex(drop_table_regex))) {
                if (searchTable(name, res[2])) {
                    if (DDL::dropTable(name, res[2])) {
                        std::cout << "Drop table successfully!" << std::endl;
                    } else {
                        status &= false;
                    }
                } else {
                    status &= false;
                }
                return status;
            }
        }

        // error
        std::cout << "Syntax error!" << std::endl;

        return status;
    }
    /*退出时清理*/
    void clear() {
        std::cout << "Bye" << std::endl;
    }
};