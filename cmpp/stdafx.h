#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             //  �� Windows ͷ�ļ����ų�����ʹ�õ���Ϣ
#include <windows.h>

#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __TODO__ __FILE__ "("__STR1__(__LINE__)") : [TODO] "