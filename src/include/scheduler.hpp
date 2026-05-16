#pragma once


#include <vector>
#include <iostream>

void SetCurrentThreadAffinity(const std::vector<int>& core_ids);