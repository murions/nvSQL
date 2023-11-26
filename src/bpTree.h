//bpTree.h

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <initializer_list>
#include <format>
#include <queue>
#include <string>
#include <vector>
#include <algorithm>
#include "type_traits.h"
#include "dataMgr.h"

using namespace std;

enum printType{KEY, DINDEX, ROOT, INDEX};

/**
 * @brief	b+Tree
 */
template <typename T>
class bpTree {
/*
 * eg.
 *    input: {3,-9}, {5, 8}, {8, 1}, {6, 4}, {0, 10}, {9, 13}, {1, 1}
 *    build:
 *      node:              +------- 6 -------+
 *                        /                   \
 *               +-- 1 --+-- 5 --+          +- 8 -+
 *              /        |        \        /       \
 *           | 0 |   | 1 | 3 |   | 5 |  | 6 |   | 8 | 9 |
 *           x - x   x - x - x   x - x  x - x   x - x - x
 *             |       |   |       |      |       |   |  
 *       head :0>>>>>>>>>1>>>>>>>>>5>>>>>>6>>>>>>>>>8>>>>tail
 *             |       |   |       |      |       |   |
 *      indexs:4       6   0       1      3       2   5
 */
public:
    /**
    * @brief	关键字结构
    */
    struct keyValue {
        using key_type = T;
        using data_type = string;

        key_type key;
        data_type data;
        keyValue() {
            this->key = key_type{};
            this->data = data_type{};
        }
        keyValue(key_type key, data_type data) {
            this->key = key;
            this->data = data;
        }
        bool operator<(keyValue &v) {
            return this->key < v.key;
        }
        bool operator>(keyValue &v) {
            return this->key > v.key;
        }
    };
private:
    /**
    * @brief	树节点
    */
    struct _node {
        using value_type = pair<typename keyValue::key_type, int>;

        vector<value_type> vals; // 节点key
        vector<_node *> nodes;   // 节点子节点
        int index; // 叶子节点对应leafs数组下标
        bool isLeaf; // 叶子节点标志
        _node() {
            vals.reserve(8);
            nodes.reserve(8);
            nodes.push_back(nullptr);
            index = -1;
            isLeaf = false;
        }
        _node(value_type v) : _node() {
            vals.push_back(v);
            nodes.push_back(nullptr);
        }
        void push_back(value_type v) {
            vals.push_back(v);
            nodes.push_back(nullptr);
        }
        void push_back() {
            vals.push_back(value_type{});
            nodes.push_back(nullptr);
        }
    };

    struct _leaf {
        _node* leaf;
        _leaf* next;
        //int data;
        _leaf(){
            this->leaf = nullptr;
            this->next = nullptr;
        }
        _leaf(_node*& node) : _leaf() {
            this->leaf = &(*(node));
        }
    };

public:
    using node = _node;
    using node_ptr = node*;
    using leaf = _leaf;
    using leaf_ptr = leaf*;

    using data_type = keyValue::data_type;
    using keyType = int;
    using node_value_t = node::value_type;

private:
    vector<leaf_ptr> leafs;
    //vector<data_type> datas;
    vector<bool> indexs;
    dataMgr dm;

protected:
    node_ptr root;
    int m = 3; // m必须大于2
    int min_num, max_num; 

public:
    string database = "1";
    string table = "t1";

    leaf_ptr head, tail;

    bpTree(){
        dm.setKeyType(keyTypeIsString<typename node_value_t::first_type>);
    };
    bpTree(string database, string table, int m = 3) noexcept {
        init(database, table, m);
    }
    ~bpTree() noexcept {
        clear();
    }

// ---------------------------------------------------------------------------

private:
    /**
     * @brief   辅助fission函数
     * @param	p       节点
     * @param	_lr     新的左节点的引用
     * @param	_rr     新的右节点的引用
     * @param	half    待求中间值位置
     */
    void _fission(node_ptr &p, node_ptr &_lr, node_ptr &_rr, int &half) {
        half = (m - 1) / 2;
        _lr = new node;
        _rr = new node;
        // 将节点的vals分为[0~half-1, half, half+1~m-1]三部分
        _lr->nodes[0] = p->nodes[0];
        for (auto i = 0; i < half; ++i) {
            _lr->push_back(p->vals[i]);
            _lr->nodes[i + 1] = p->nodes[i + 1];
        }
        if(p->isLeaf){
            _rr->nodes[0] = p->nodes[half];
            _rr->push_back(p->vals[half]);
        }
        for (auto i = half + 1; i < m; ++i) {
            _rr->nodes[i - half - 1] = p->nodes[i];
            _rr->push_back(p->vals[i]);
        }
        _rr->nodes.back() = p->nodes.back();
        _lr->isLeaf = p->isLeaf;
        _rr->isLeaf = p->isLeaf;
    }

    /**
     * @brief	前序遍历替代层序遍历，为方便显示层结构未直接使用队列层序遍历
     * @param	r       节点
     * @param	v       存储层节点的数组
     * @param	depth   当前递归深度
     */
    void _levelOrder(node_ptr r, vector<vector<node_ptr>> &v, size_t depth = 0) {
        if (r == nullptr)
            return;
        // 前序遍历，将当前层所有节点压入存储数组的对应位置
        if (v.size() <= depth)
            v.push_back(vector<node_ptr> {r});
        else
            v[depth].push_back(r);
        for (auto i : r->nodes) {
            _levelOrder(i, v, depth + 1);
        }
    }

    /**
     * @brief	辅助erase函数，与右侧最近关键字交换
     * @param	r_swap      递归节点
     * @param	v_delete    带有v的原节点
     */
    void _swap(node_ptr &r_swap, node_value_t& v_delete){
        // 优先与右最近关键字交换
        if(r_swap->isLeaf){
            auto _tmp = v_delete;
            v_delete = r_swap->vals.front();
            r_swap->vals.insert(r_swap->vals.begin(), _tmp);
            r_swap->nodes.push_back(nullptr);
            return;
        }
        _swap(r_swap->nodes.front(), v_delete);
    }

protected:
    /**
     * @brief	节点分裂
     * @param	r   节点
     */
    void fission(node_ptr &r) {
        if (r == nullptr)
            return;
        if (r == root) { // 根节点，直接向上生长一层，不考虑对树结构的影响
            auto &p = r;
            if (p == nullptr)
                return;
            if ((int)p->nodes.size() > m) {
                node_ptr _lr, _rr;
                int half;
                auto p_index = p->index;
                _fission(p, _lr, _rr, half);
                auto _r = new node(r->vals[half]);
                _r->nodes[0] = _lr;
                _r->nodes[1] = _rr;
                auto _p = p;
                p = _r;

                delete _p;
                
                if(_lr->isLeaf){ // 将新插入的叶子节点加入到leafs中
                    leafs[p_index]->leaf = &(*(r->nodes[0]));
                    auto __q = new leaf(r->nodes[1]);
                    __q->next = leafs[p_index]->next;
                    leafs[p_index]->next = __q;
                    r->nodes[0]->index = p_index;
                    r->nodes[1]->index = leafs.size();
                    leafs.push_back(__q);
                }
                return;
            }
        }
        // 非根节点，将中间值插入父节点，该节点分裂为两份
        auto n = r->nodes.size();
        for (auto k = 0uz; k < n; ++k) {
            auto &p = r->nodes[k];
            if (p == nullptr)
                continue;
            if ((int)p->nodes.size() > m) {
                node_ptr _lr, _rr;
                int half;
                auto p_index = p->index;
                _fission(p, _lr, _rr, half);
                r->vals.push_back(p->vals[half]);
                sort(r->vals.begin(), r->vals.end());
                auto _p = p;
                p = _lr;
                r->nodes.insert(r->nodes.begin() + k + 1, _rr);
                delete _p;
                
                if(r->nodes[k]->isLeaf){ // 将新插入的叶子节点加入到leafs中
                    leafs[p_index]->leaf = &(*(r->nodes[k]));
                    auto __q = new leaf(r->nodes[k + 1]);
                    __q->next = leafs[p_index]->next;
                    leafs[p_index]->next = __q;
                    r->nodes[k]->index = p_index;
                    r->nodes[k + 1]->index = leafs.size();
                    leafs.push_back(__q);
                }
            }
        }
    }
    
    /**
     * @brief	插入节点
     * @param	r   节点
     * @param	v   关键字
     */
    void _insert(node_ptr& r, node_value_t& v, int& _pos) {
        if (r == nullptr) { // 新创建一个节点
            r = new node(v);
            r->isLeaf = true;
            // 将新插入的节点当作head节点，并设置head在leafs中的索引
            head->leaf = &(*(r));
            r->index = 0;
            return;
        }
        if (r->isLeaf) { // 叶子节点，按顺序插入
            auto n = r->vals.size();
            if (n == 1) {
                if (r->vals[0].first == v.first){ // 相同key，直接替换值
                    //r->vals[0].second = v.second;
                    _pos = r->vals[0].second;
                    return;
                }
                if (r->vals[0].first < v.first) {
                    r->vals.insert(r->vals.begin() + 1, v);
                    r->nodes.push_back(nullptr);
                } else {
                    r->vals.insert(r->vals.begin(), v);
                    r->nodes.push_back(nullptr);
                }
            } else {
                for (auto i = 0uz; i < n - 1; ++i) {
                    if (v.first < r->vals[0].first) {
                        r->vals.insert(r->vals.begin(), v);
                        r->nodes.push_back(nullptr);
                    } else if(v.first == r->vals[0].first){ // 相同key，直接替换值
                        _pos = r->vals[0].second;
                        return;
                    } else if (v.first >= r->vals[n - 1].first) {
                        if (r->vals[n - 1].first == v.first){ // 相同key，直接替换值
                            //r->vals[n - 1].second = v.second;
                            _pos = r->vals[n - 1].second;
                            return;
                        }
                        r->push_back(v);
                        break;
                    } else if (r->vals[i].first < v.first && r->vals[i + 1].first >= v.first) {
                        if (r->vals[i + 1].first == v.first){ // 相同key，直接替换值
                            //r->vals[i + 1].second = v.second;
                            _pos = r->vals[i + 1].second;
                            return;
                        }
                        r->vals.insert(r->vals.begin() + i + 1, v);
                        r->nodes.push_back(nullptr);
                        break;
                    }
                }
            }
            return;
        }
        if (v.first == r->vals[0].first) { // 相同key，直接替换值
            //r->vals[0].second = v.second;
            if(!r->isLeaf)
                _insert(r->nodes[1], v, _pos);
            return;
        }
        if (v.first < r->vals[0].first) { // 转向最左边
            _insert(r->nodes[0], v, _pos);
        } else if (v.first >= r->vals.back().first) { // 转向最右边
            if (v.first == r->vals.back().first) { // 相同key，直接替换值
                //r->vals.back().second = v.second;
                if(!r->isLeaf)
                    _insert(r->nodes.back(), v, _pos);
                return;
            }
            _insert(r->nodes.back(), v, _pos);
        }
        else {
            for (auto i = 1uz; i < r->vals.size(); ++i) {
                if (v.first == r->vals[i - 1].first) { // 相同key，直接替换值
                    //r->vals[i].second = v.second;
                    if(!r->isLeaf)
                        _insert(r->nodes[i], v, _pos);
                    return;
                }
                if (v.first >= r->vals[i - 1].first && v.first < r->vals[i].first) { // 转向下一层
                    _insert(r->nodes[i], v, _pos);
                }
            }
        }
        // 分裂
        fission(r);
    }
    /**
     * @brief	对_insert的封装
     * @param	v
     */
    void insert(node_value_t v, int& _pos){
        // 插入并修正
        _insert(root, v, _pos);
        fission(root);
    }

    /**
     * @brief	删除节点（删除叶子节点或非叶子节点）
     * @param	r       传入节点
     * @param	v       删除节点值
     * @param   _found  是否找到 
     * @return	0       处理完毕
     * @return  1       待处理
     * @return  2       上一次查找结果
     */
    int _erase(node_ptr &r, node_value_t::first_type v, bool& _found, bool& _first){
        if(r == nullptr){
            return 0;
        }
        bool found = false;
        for(auto i = 0uz; i < r->vals.size(); ++i){
            if(v == r->vals[i].first){
                if(!r->isLeaf){ // 非根节点
                    if(!_first){ // 叶子节点的值还未删除，先删除叶子节点
                        // 转向右节点
                        auto _res = _erase(r->nodes[i + 1], v, _found, _first);
                        if(_res == 2){
                            found = true;
                            _first = true;
                            break;
                        }
                        else{
                            break;
                        }
                    }
                    // 将v与key值最接近的右关键字交换位置，进入右分支后删除已是叶子节点的v
                    _swap(r->nodes[i + 1], r->vals[i]);
                    auto _res = _erase(r->nodes[i + 1], v, _found, _first);
                    if(_res == 2){
                        found = true;
                        _first = true;
                        break;
                    }
                    else if(_res == 0){
                        break;
                    }
                }
                else{ // 找到该叶子节点，返回到父亲节点
                    _found = true;
                    indexs[r->vals[i].second] = false;
                    return 2;
                }
            }
        }
        for(auto i = 0uz; i <= r->vals.size(); ++i){ // 查找v
            if(i == 0){ // 进入第一个子节点
                if(v < r->vals.front().first){
                    auto _res = _erase(r->nodes.front(), v, _found, _first);
                    if(_res == 2){
                        found = true;
                        _first = true;
                        break;
                    }
                    else if(_res == 0){
                        break;
                    }
                }
            }
            else if(i == r->vals.size()){ // 进入最后一个子节点
                if(v >= r->vals.back().first){
                    auto _res = _erase(r->nodes.back(), v, _found, _first);
                    if(_res == 2){
                        found = true;
                        _first = true;
                        break;
                    }
                    else if(_res == 0){
                        break;
                    }
                }
            }
            else{ // 进入中间节点
                if(v >= r->vals[i - 1].first && v < r->vals[i].first){
                    auto _res = _erase(r->nodes[i], v, _found, _first);
                    if(_res == 2){
                        found = true;
                        _first = true;
                        break;
                    }
                    else if(_res == 0){
                        break;
                    }
                }
            }
        }
        // 查找结束，检测该树是否存在v
        if(found){ // r为所寻节点的父节点
            // 找到要处理的节点 r->nodes[i]
            int pos = -1;
            int n = r->nodes.size();
            for(auto i = 0; i < n; ++i){
                for(auto j : r->nodes[i]->vals){
                    if(j.first == v){
                        pos = i;
                        break;
                    }
                }
                if(pos != -1)
                    break;
            }
            // 标记其兄弟节点
            int lb = pos == 0 ? -1 : pos - 1;
            int rb = pos == n - 1 ? -1 : pos + 1;
            auto check2 = [this](node_value_t& p, node_ptr& p_r)->bool{
                return (p.first != p_r->vals.front().first) || ((p.first == p_r->vals.front().first)
                     && ((int)p_r->vals.size() - 1 > min_num));
            };
            // 1、自身富裕
            if((int)r->nodes[pos]->vals.size() > min_num){ // 直接删除自身一个节点，处理完毕
                // cout << "自身富裕" << endl;
                // 删除v
                int _pos = -1;
                auto& q = r->nodes[pos];
                for(auto i = 0uz; i < q->vals.size(); ++i){
                    if(q->vals[i].first == v){
                        _pos = i;
                        break;
                    }
                }
                q->vals.erase(q->vals.begin() + _pos);
                q->nodes.pop_back();
                return 0;
            }
            // 2、右兄弟富裕
            else if(rb > -1 && (int)r->nodes[rb]->vals.size() > min_num && check2(r->vals[pos], r->nodes[rb])){ // 摘下右兄弟的一个节点，处理完毕
                // cout << "右兄弟富裕" << endl;
                bool _eq = (r->vals[pos].first == r->nodes[rb]->vals.front().first);
                if(_eq){
                    r->nodes[rb]->vals.erase(r->nodes[rb]->vals.begin());
                    r->nodes[rb]->nodes.erase(r->nodes[rb]->nodes.begin());
                }
                // 从右兄弟处取走第一个值
                auto rv = r->nodes[rb]->vals.front();
                r->nodes[rb]->vals.erase(r->nodes[rb]->vals.begin());
                r->nodes[rb]->nodes.pop_back();
                // 将来自右兄弟的值交给父节点
                auto hv = r->vals[pos];
                r->vals[pos] = rv;
                // 删除v，并用来自父节点的值替代v
                int _pos = -1;
                auto& q = r->nodes[pos];
                q->vals.push_back(hv);
                for(auto i = 0uz; i < q->vals.size(); ++i){
                    if(q->vals[i].first == v){
                        _pos = i;
                        break;
                    }
                }
                q->vals.erase(q->vals.begin() + _pos);
                if(_eq){
                    r->nodes[rb]->vals.insert(r->nodes[rb]->vals.begin(), r->vals[pos]);
                    r->nodes[rb]->nodes.push_back(nullptr);
                }
                return 0;
            }
            // 3、左兄弟富裕
            else if(lb > -1 && (int)r->nodes[lb]->vals.size() > min_num){ // 摘下左兄弟的一个节点，处理完毕
                // cout << "左兄弟富裕" << endl;
                // 从左兄弟处取走最后一个值
                auto lv = r->nodes[lb]->vals.back();
                r->nodes[lb]->vals.pop_back();
                r->nodes[lb]->nodes.pop_back();
                // 将来自左兄弟的值交给父节点
                auto hv = r->vals[lb];
                r->vals[lb] = lv;
                // 删除v，并用来自父节点的值替代v
                int _pos = -1;
                auto& q = r->nodes[pos];
                q->vals.insert(q->vals.begin(), hv);
                for(auto i = 0uz; i < q->vals.size(); ++i){
                    if(q->vals[i].first == v){
                        _pos = i;
                        break;
                    }
                }
                q->vals.erase(q->vals.begin() + _pos);
                if(r->vals[lb].first != q->vals.front().first){
                    q->vals.insert(q->vals.begin(), r->vals[lb]);
                    q->nodes.push_back(nullptr);
                }
                return 0;
            }
            // 4、父亲富裕或不富裕（若导致节点缺少，直接抛给上级节点处理）
            else{ // 摘下父亲的一个节点，处理完毕
                // cout << "父亲富裕" << endl;
                bool _eq = false;
                // 删除v
                int _pos = -1;
                auto& q = r->nodes[pos];
                for(auto i = 0uz; i < q->vals.size(); ++i){
                    if(q->vals[i].first == v){
                        _pos = i;
                        break;
                    }
                }
                q->vals.erase(q->vals.begin() + _pos);
                q->nodes.pop_back();
                // 合并
                if(rb > -1){ // 向右合并
                    _eq = (r->vals[pos].first == r->nodes[rb]->vals.front().first);
                    auto p_index = r->nodes[pos]->index;
                    if(r->nodes[pos]->isLeaf){ // 修正leafs节点对叶子节点的链接
                        auto __p = leafs[p_index]->next;
                        leafs[p_index]->next = __p->next;
                        delete __p;
                    }
                    auto hv = r->vals[pos];
                    auto lp = r->nodes[pos];
                    auto rp = r->nodes[rb];
                    if(_eq){
                        lp->nodes.pop_back();
                    }
                    else{
                        lp->vals.push_back(hv);
                    }
                    for(auto i : rp->vals){
                        lp->vals.push_back(i);
                    }
                    for(auto i : rp->nodes){
                        lp->nodes.push_back(i);
                    }
                    r->vals.erase(r->vals.begin() + pos);
                    r->nodes.erase(r->nodes.begin() + rb);
                    delete rp;
                }
                else if(lb > -1){ // 向左合并
                    _eq = (r->vals[lb].first == r->nodes[pos]->vals.front().first);
                    auto p_index = r->nodes[pos]->index;
                    if(r->nodes[pos]->isLeaf){ // 修正leafs节点对叶子节点的链接
                        auto __p = leafs[p_index]->next;
                        leafs[p_index]->leaf = __p->leaf;
                        leafs[p_index]->next = __p->next;
                        delete __p;
                        swap(leafs[p_index], __p);
                    }
                    auto hv = r->vals[lb];
                    auto lp = r->nodes[lb];
                    auto rp = r->nodes[pos];
                    if(_eq){
                        lp->nodes.pop_back();
                    }
                    else{
                        lp->vals.push_back(hv);
                    }
                    for(auto i : rp->vals){
                        lp->vals.push_back(i);
                    }
                    for(auto i : rp->nodes){
                        lp->nodes.push_back(i);
                    }
                    r->vals.erase(r->vals.begin() + lb);
                    r->nodes.erase(r->nodes.begin() + pos);
                    delete rp;
                }
                if(r == root && r->vals.size() == 0){ // 若树只有两层且根节点原本只有1个关键字，处理后根节点会没有值，直接减掉一层
                    auto _r = r;
                    r = r->nodes.front();
                    delete _r;
                    return 0;
                }
                if((int)r->vals.size() > min_num)
                    return 0;
                else
                    return 1;
            }
        }
        // 对上次递归抛出的中间层节点关键字数目缺失进行处理，此时不可能为叶子节点
        if(_found && !r->isLeaf){
            int n = r->nodes.size();
            bool lack = false;
            int pos = -1;
            for(auto i = 0; i < n; ++i){
                if((int)r->nodes[i]->vals.size() < min_num){
                    pos = i;
                    lack = true;
                    break;
                }
            }
            if(lack){ // r为上次处理节点p的父节点的父节点, 子节点贫穷
                // 标记p父节点的兄弟节点
                int lb = pos == 0 ? -1 : pos - 1;
                int rb = pos == n - 1 ? -1 : pos + 1;
                // 1、p父节点的右兄弟富裕
                if(rb > -1 && (int)r->nodes[rb]->vals.size() > min_num){ // 摘下右兄弟一个节点，处理完毕
                    // cout << "右叔叔富裕" << endl;
                    // 从右兄弟处取走第一个值
                    auto rv = r->nodes[rb]->vals.front();
                    r->nodes[rb]->vals.erase(r->nodes[rb]->vals.begin());
                    auto rn = r->nodes[rb]->nodes.front();
                    r->nodes[rb]->nodes.erase(r->nodes[rb]->nodes.begin());
                    // 将来自右兄弟的值交给父节点
                    auto hv = r->vals[pos];
                    r->vals[pos] = rv;
                    // 插入来自父节点的值，接入来自右兄弟的节点
                    auto& q = r->nodes[pos];
                    q->vals.push_back(hv);
                    q->nodes.push_back(rn);
                    return 0;
                }
                // 2、p父节点的左兄弟富裕
                else if(lb > -1 && (int)r->nodes[lb]->vals.size() > min_num){ // 摘下左兄弟一个节点，处理完毕
                    // cout << "左叔叔富裕" << endl;
                    // 从左兄弟处取走最后一个值
                    auto lv = r->nodes[lb]->vals.back();
                    r->nodes[lb]->vals.pop_back();
                    auto ln = r->nodes[lb]->nodes.back();
                    r->nodes[lb]->nodes.pop_back();
                    // 将来自左兄弟的值交给父节点
                    auto hv = r->vals[lb];
                    r->vals[lb] = lv;
                    // 插入来自父节点的值，接入来自左兄弟的节点
                    auto& q = r->nodes[pos];
                    q->vals.insert(q->vals.begin(), hv);
                    q->nodes.insert(q->nodes.begin(), ln);
                    return 0;
                }
                // 3、p父节点的父亲富裕或不富裕（若导致节点缺少，直接抛给上级节点处理）
                else{ // 摘下父亲一个节点，若原来父亲富裕则处理完毕，否则抛给上级处理
                    // cout << "祖父富裕" << endl;
                    // 合并
                    if(rb > -1){ // 向右合并
                        auto hv = r->vals[pos];
                        auto lp = r->nodes[pos];
                        auto rp = r->nodes[rb];
                        lp->vals.push_back(hv);
                        for(auto i : rp->vals){
                            lp->vals.push_back(i);
                        }
                        for(auto i : rp->nodes){
                            lp->nodes.push_back(i);
                        }
                        r->vals.erase(r->vals.begin() + pos);
                        r->nodes.erase(r->nodes.begin() + rb);
                        delete rp;
                    }
                    else if(lb > -1){ // 向左合并
                        auto hv = r->vals[lb];
                        auto lp = r->nodes[lb];
                        auto rp = r->nodes[pos];
                        lp->vals.push_back(hv);
                        for(auto i : rp->vals){
                            lp->vals.push_back(i);
                        }
                        for(auto i : rp->nodes){
                            lp->nodes.push_back(i);
                        }
                        r->vals.erase(r->vals.begin() + lb);
                        r->nodes.erase(r->nodes.begin() + pos);
                        delete rp;
                    }
                    if(r == root && r->vals.size() == 0){ // 若树只有两层且根节点原本只有1个关键字，处理后根节点会没有值，直接减掉一层
                        auto _r = r;
                        r = r->nodes.front();
                        delete _r;
                        return 0;
                    }
                    if((int)r->vals.size() > min_num){
                        return 0;
                    }
                    else{
                        return 1;
                    }
                }
            }
        }
        return 1;
    }

    /**
     * @brief	递归查找
     * @param	r           节点
     * @param	v           关键字
     * @return	node_ptr*   含关键字的节点
     */
    node_ptr* _find(node_ptr& r, node_value_t::first_type v) const {
        if(r == nullptr){
            return nullptr;
        }
        auto n = r->vals.size();
        if(r->isLeaf){
            for(auto i = 0uz; i < n; ++i){
                if(v == r->vals[i].first){ // 已找到
                    return &r;
                }
            }
        }
        for(auto i = 0uz; i <= n; ++i){
            if(i == 0){ // 进入第一个子节点
                if(v < r->vals.front().first){
                    return _find(r->nodes.front(), v);
                }
            }
            else if(i == n){ // 进入最后一个子节点
                if(v >= r->vals.back().first){
                    return _find(r->nodes.back(), v);
                }
            }
            else{ // 进入中间节点
                if(v >= r->vals[i - 1].first && v < r->vals[i].first){
                    return _find(r->nodes[i], v);
                }
            }
        }
        // 未找到
        return nullptr;
    }

    /**
     * @brief	层序遍历并打印
     * @param	r   节点
     */
    void levelOrder(node_ptr r, printType t = KEY) {
        vector<vector<node_ptr>> res;
        _levelOrder(r, res);
        for (auto i : res) {
            for (auto j : i) {
                for (auto k : j->vals) {
                    if(t == KEY)
                        cout << k.first << " ";
                    else if(t == DINDEX)
                        cout << k.second << " ";
                    else if(t == ROOT)
                        cout << j->isLeaf << " ";
                    else if(t == INDEX)
                        cout << j->index << " ";
                }
                cout << "\b; ";
            }
            cout << endl;
        }
    }

    void levelOrder(bpTree::node_ptr r, string& o){
        if(r == nullptr){
            o = "";
            return;
        }
        bool _leaf = false;
        if(r->isLeaf){ // 只有一层节点
            o += "$ ";
            for(auto i : r->vals){
                o += (toStr(i.first)() + ",");
            }
            o.back() = ' ';

            return;
        }
        queue<bpTree::node_ptr> q;
        q.push(r);
        q.push(nullptr);
        while(!q.empty()){
            auto p = q.front();
            q.pop();
            if(p == nullptr){
                auto t = q.front();
                if(!_leaf){
                    if(t->isLeaf){
                        o += "$ ";
                        _leaf = true;
                    }
                    else{
                        o += "# ";
                    }
                }
                else
                    o += "# ";
                if(!q.empty()){
                    q.push(nullptr);
                }
                continue;
            }
            for(auto i : p->vals)
                o += (toStr(i.first)() + ",");
            o.back() = ' ';
            if(!p->isLeaf){
                for(auto i : p->nodes){
                    q.push(i);
                }
            }
        }
    }

    void rebuild(bpTree::node_ptr& r, const vector<string>& _input){
        if(_input.size() == 1 && _input[0] == ""){
            return;
        }
        size_t pos = 0uz;
        bool _leaf = false;
        bool _beg = true;
        queue<bpTree::node_ptr*> q;
        q.push(&r);
        while(!q.empty()){
            if(pos + 1 > _input.size())
                break;
            if(_input[pos] == "#"){
                ++pos;
                continue;
            }
            if(_input[pos] == "$"){
                _leaf = true;
                ++pos;
                continue;
            }
            auto p = q.front();
            q.pop();
            (*p) = new bpTree::node;
            if(_leaf){
                (*p)->isLeaf = true;
                if(_beg){ // 初始状态
                    head->leaf = &(*(*p));
                    _beg = false;
                }
                else{
                    leafs.push_back(new leaf(*p));
                }
                if(leafs.size() == 3){
                    leafs.back()->next = tail;
                    head->next = leafs.back();
                }
                else if(leafs.size() > 3){
                    leafs.back()->next = tail;
                    leafs[leafs.size() - 2]->next = leafs.back();
                }
            }
            vector<string> _vals;
            str_split(_input[pos], _vals, ',');
            for(auto _i : _vals){
                (*p)->vals.push_back(bpTree::node_value_t(keyFormatConverter<typename node_value_t::first_type>(_i)(), -1));
                (*p)->nodes.push_back(nullptr);
            }
            for(auto& _i : (*p)->nodes){
                q.push(&_i);
            }
            ++pos;
        }
        int _cnt = 0;
        for(auto itr = head; itr != tail; itr = itr->next){
            itr->leaf->index = _cnt++;
            if(_cnt == 1){
                ++_cnt;
            }
        }
    }
    
    /**
     * @brief	递归删除r内的所有节点
     * @param	r   节点
     */
    void _clear(node_ptr r) {
        if (r == nullptr) {
            return;
        }
        for (auto i : r->nodes) {
            _clear(i);
        }
        delete r;
    }

public:
    /**
     * @brief	创建新树
     * @param	v   键值对数组
     * @param	m   树阶数
     */
    void clear_all(int m = 3){
        clear();
        root = nullptr;
        head = new leaf;
        tail = new leaf;
        head->next = tail;
        leafs.push_back(head);
        leafs.push_back(tail);
        // 计算每个节点的最少与最多关键字数
        this->m = m;
        min_num = (m + 1) / 2 - 1;
        max_num = m;
    }
    void clear_all(vector<keyValue> &v, int m = 3) {
        clear_all(m);
        for (auto i : v) { // 依次插入
            insert(i);
        }
    }
    void init(string database, string table, int m = 3) {
        root = nullptr;
        min_num = (m + 1) / 2 - 1;
        max_num = m;
        head = new leaf;
        tail = new leaf;
        head->next = tail;
        leafs.push_back(head);
        leafs.push_back(tail);

        this->m = m;
        this->database = database;
        this->table = table;
        
        dm.setKeyType(keyTypeIsString<typename node_value_t::first_type>);
        dm.init(database, table);
        
        string _ser = "";
        string _indexs = "";
        
        string filename = dataPos + database + "/" + table + ".ind";
        if(filesystem::exists(filename)){
            ifstream file(filename, ios::binary | ios::in);
            getline(file, _ser);
            getline(file, _indexs);
            deSerialize(_ser);
            for(auto i : _indexs){
                indexs.push_back((i == 0 ? false : true));
            }
            for(auto it = head; it != tail; it = it->next){
                for(auto& i : it->leaf->vals){
                    int sz = 0;
                    file.read((char*)&sz, 4);
                    if(keyTypeIsString<typename node_value_t::first_type>){
                        char iFirst[sz - 3];
                        iFirst[sz - 4] = '\0';
                        file.read(iFirst, sz - 4);
                        string ifst(iFirst);
                        setIFirst(i.first, ifst);
                    }
                    else{
                        int iFirst = 0;
                        file.read((char*)&iFirst, 4);
                        i.first = iFirst;
                    }
                    int iSecond = 0;
                    file.read((char*)&iSecond, 4);
                    setIFirst(i.second, iSecond);
                }
            }
            file.close();
        }
        else{
            return;
        }
    }

    /**
     * @brief	插入节点
     * @param	v   键值对
     */
    void insert(keyValue v){
        indexs.push_back(true);
        node_value_t _v(v.key, indexs.size() - 1);
        int _pos = -1;
        insert(_v, _pos);
        if(_pos >= 0){
            dm.updateRecord(_pos, v.data);
            indexs[_pos] = true;
            indexs.pop_back();
        }
        else{
            dm.createRecord(v.data);
        }
    }
    
    /**
     * @brief	删除节点
     * @param	v   键值对
     */
    bool erase(node_value_t::first_type v){
        for(auto iter = head; iter != tail; iter = iter->next){
            for(auto i : iter->leaf->vals){
                if(i.first == v){
                    dm.deleteRecord(i.second);
                }
            }
        }
        if(root->isLeaf){ // 节点只有根节点
            int n = root->vals.size();
            for(auto i = 0; i < n; ++i){
                if(root->vals[i].first == v){ // 直接删除
                    indexs[root->vals[i].second] = false;
                    root->vals.erase(root->vals.begin() + i);
                    root->nodes.pop_back();
                    return true;
                }
            }
            return false;
        }
        // 递归删除
        bool _found = false, _first = false;
        _erase(root, v, _found, _first);
        //traverse();
        _first = true;
        _erase(root, v, _found, _first);
        if(!_found){
            return false;
        }
        return true;
    }

    
    /**
     * @brief	查找键值对所在节点
     * @param	v           key
     * @return	node_ptr*   节点
     */
    node_ptr* getNode(node_value_t::first_type v){
        return _find(root, v);
    }

    /**
     * @brief	查找键值对的值
     * @param	key     key
     * @param	data    返回key对应的值
     * @return	true    查找成功
     * @return	false   查找失败
     */
    // template <input_type<keyValue> findType>
    // bool findData(findType key, data_type*& data){
    //     auto vKey = get_key<keyValue, findType>(key)();
    //     auto res = getNode(vKey);
    //     if(res == nullptr)
    //         return false;
    //     for(auto& i : (*res)->vals){
    //         if(i.first == vKey){
    //             data = &(datas[i.second]);
    //             return true;
    //         }
    //     }
    //     return false;
    // }

    /**
    * @brief	更改关键字值
    * @param	v       // 键值对
    * @param	data    // data
    */
    bool update(node_value_t::first_type key, data_type data){
        // data_type *_data = nullptr;
        // auto _res = findData(key, _data);
        // if(!_res){
        //     return false;
        // }
        // *_data = data;
        
        for(auto itr = head; itr != tail; itr = itr->next){
            for(auto i : itr->leaf->vals){
                if(i.first == key){
                    dm.updateRecord(i.second, data);
                    return true;
                }
            }
        }
        
        return false;
    }

    string find(node_value_t::first_type key){
        string res = "";    
        auto _n = getNode(key);
        int pos = 0;
        if(_n == nullptr){
            return res;
        }
        for(auto& i : (*_n)->vals){
            if(i.first == key){
                pos = i.second;
            }
        }
        dm.readRecord(res, pos);
        return res;
    }

    // string getData(keyType key){
    //     data_type *_data;
    //     findData(key, _data);
    //     return _data->data;
    // }

    /**
     * @brief	层序遍历并打印每层结构
     */
    void traverse(printType t = KEY) {
        cout << "-----------------------------" << endl;
        levelOrder(root, t);
        cout << "-----------------------------" << endl;
    }

    void print(printType t = KEY){
        for(auto iter = head; iter != tail; iter = iter->next){
            for(auto i : iter->leaf->vals){
                if(t == KEY)
                    cout << i.first << " ";
            }
            cout << "; ";
        }
        cout << endl;
    }

    /**
     * @brief	清空bTree
     * @param	r
     */
    void clear() {
        leafs.clear();
        auto p = head, q = p;
        while(p != nullptr){
            q = p;
            p = p->next;
            delete q;
        }
        _clear(root);
    }

    string serialize(){
        string output = "";
        levelOrder(root, output);

        return output;
    }

    void save(){
        string _res = serialize();

        string filename = dataPos + database + "/" + table + ".ind";
        ofstream file(filename, ios::binary | ios::out);
        file.write(_res.c_str(), _res.size());
        file.write("\n", 1);
        for(auto i : indexs){
            char t[1] = {};
            t[0] = (i ? 1 : 0);
            file.write(t, 1);
        }
        file.write("\n", 1);
        for(auto itr = head; itr != tail; itr = itr->next){
            for(auto i : itr->leaf->vals){
                int sz = getKeySize(i.first)() + 4;
                file.write((char*)&sz, 4);
                file.write((getKeyData(i.first)()), sz - 4);
                file.write((char*)&i.second, 4);
            }
        }
        file.close();
    }

    void deSerialize(string& s){
        vector<string> _res;
        str_split(s, _res, ' ');
        clear_all(3);
        rebuild(root, _res);
    }

    void recordInit(){
        dm.profInit();
    }
};