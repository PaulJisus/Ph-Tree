#include "phtree.h"
#include "phtree_multimap.h"
#include <chrono>
#include <iostream>
#include <set>

using namespace improbable::phtree;

int relocate_example() {
    auto tree = PhTreeMultiMapD<2, int, ConverterMultiply<2, 1, 500>, std::unordered_set<int>>();
    std::vector<PhPointD<2>> vecPos;
    int dim = 2000;

    int num = 50000;
    for (int i = 0; i < num; ++i) {
        PhPointD<2> p = {(double)(rand() % dim), (double)(rand() % dim)};
        vecPos.push_back(p);
        tree.emplace(p, i);
    }

    long T = 0;
    int nT = 0;
    while (true) {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num; ++i) {
            PhPointD<2>& p = vecPos[i];
            PhPointD<2> newp = {p[0] + 1, p[1] + 1};
            tree.relocate(p, newp, i, false);
            p = newp;
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        auto s = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        ++nT;
        T += (long)s.count() / 1000;
        std::cout << s.count() << "    " << (T / nT)
                  << "     msec/num= " << (s.count() / (double)num) << std::endl;
    }

    return 0;
}

int main() {
    std::cout << "PH-Tree example with 3D `double` coordinates." << std::endl;
    PhPointD<3> p1({1, 1, 1});
    PhPointD<3> p2({2, 2, 2});
    PhPointD<3> p3({3, 3, 3});
    PhPointD<3> p4({4, 4, 4});

    PhTreeD<3, int> tree;
    tree.emplace(p1, 1);
    tree.emplace(p2, 2);
    tree.emplace(p3, 3);
    tree.emplace(p4, 4);

    std::cout << "All values:" << std::endl;
    for (auto it : tree) {
        std::cout << "    id=" << it << std::endl;
    }
    std::cout << std::endl;

    std::cout << "All points in range:" << p2 << "/" << p4 << std::endl;
    for (auto it = tree.begin_query({p2, p4}); it != tree.end(); ++it) {
        std::cout << "    " << it.second() << " -> " << it.first() << std::endl;
    }
    std::cout << std::endl;

    PhPointD<3> p4b({4, 4, 4});
    tree.emplace(p4b, 5);

    std::cout << "ID at " << p4b << ": " << tree.find(p4b).second() << std::endl;

    return 0;
}

