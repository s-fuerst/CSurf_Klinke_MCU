/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "math.h"

template<typename K, typename V>
static K getNthKeyFromMap(unsigned int iPos, std::map<K,V>* pMap) {
        ASSERT(iPos < pMap->size());
        typename std::map<K,V>::iterator iterMap;
        for (iterMap = pMap->begin(); iPos > 0; ++iterMap)
                iPos--;
        return (*iterMap).first;
}

template<typename K, typename V>
static V getNthValueFromMap(unsigned int iPos, std::map<K,V>* pMap) {
        ASSERT(iPos < pMap->size());
        typename std::map<K,V>::iterator iterMap;
        for (iterMap = pMap->begin(); iPos > 0; ++iterMap)
                iPos--;
        return (*iterMap).second;
}


template<typename K, typename V>
static int findIndexFromKeyInMap(K key, std::map<K,V>* pMap) {
        int i = 0;
        for (typename std::map<K,V>::iterator iterMap = pMap->begin(); iterMap != pMap->end(); ++iterMap) {
                if (fabs((*iterMap).first - key) < 0.001) {
                        return i;
                }
                i++;
        }
        return -1;
}

