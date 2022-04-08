/*
    Definately !NOT stolen from plugin-sdk
    Credits: Plugin-SDK Team
*/
#pragma once
#include <sstream>
#include <vector>
#include "pch.h"
#include "modules/core.h"

class CRunningScriptSA 
{
public:
    CRunningScriptSA *m_pNext;
    CRunningScriptSA *m_pPrev;
    char             m_szName[8];
    unsigned char   *m_pBaseIP;
    unsigned char   *m_pCurrentIP;
    unsigned char   *m_apStack[8];
    unsigned short  m_nSP;
    char            _pad3A[2];
    void*       	m_aLocalVars[32];
    int             m_anTimers[2];
    bool            m_bIsActive;
    bool            m_bCondResult;
    bool            m_bUseMissionCleanup;
    bool            m_bIsExternal;
    bool            m_bTextBlockOverride;
    char            _padC9[3];
    int             m_nWakeTime;
    unsigned short  m_nLogicalOp;
    bool            m_bNotFlag;
    bool            m_bWastedBustedCheck;
    bool            m_bWastedOrBusted;
    char            _padD5[3];
    unsigned char  *m_pSceneSkipIP;
    bool            m_bIsMission;
    char            _padDD[3];
};

class CRunningScript 
{
public:
    CRunningScript *m_pNext;
    CRunningScript *m_pPrev;
    char            m_szName[8];
    int             m_nIp;
    int             m_anStack[6];
    unsigned short  m_nSP;
    char            _pad2E[2];
    void*            m_aLocalVars[16];
    int             m_anTimers[2];
    bool            m_bIsActive;
    bool            m_bCondResult;
    bool            m_bUseMissionCleanup;
    bool            m_bAwake;
    int             m_nWakeTime;
    unsigned short  m_nLogicalOp;
    bool            m_bNotFlag;
    bool            m_bWastedBustedCheck;
    bool            m_bWastedOrBusted;
    bool            m_bIsMission;
    char _pad86[2];

    char ProcessOneCommand();
};

// Basic wrapper around char buffer
class ScriptBuffer
{
private:
    unsigned char* _buf = nullptr;
    size_t _size = 0;
    size_t _capacity = 0;

    void _resize(size_t new_size)
    {
        if (new_size > _capacity)
        {
            _capacity += 32;
            _buf = (unsigned char*)realloc(_buf, _capacity);
        }
    }

public:
    ScriptBuffer(unsigned short opcode_id)
    {
        if (_buf)
        {
            delete[] _buf;
        }

        _buf = new unsigned char[32];
        memset(_buf, NULL, 32);
        _capacity = 32;
        memcpy(_buf, &opcode_id, 2);
        _size = 2;
    };

    ~ScriptBuffer()
    {
        delete[] _buf;
    }
    
    unsigned char* get()
    {
        return _buf;
    }
    
    template <typename T>
    void add_bytes(T value)
    {
        _resize(_size + sizeof(T) + 1); // +1 for type descriptor

        if (std::is_same_v<T, char> || std::is_same_v<T, unsigned char> || std::is_same_v<T, bool>)
        {   
            memset(&_buf[_size], 0x4, 1);
            _size += 1;
        }
        if (std::is_same_v<T, int> || std::is_same_v<T, unsigned int>)
        {   
            memset(&_buf[_size], 0x1, 1);
            _size += 1;
        }
        if (std::is_same_v<T, short> || std::is_same_v<T, unsigned short>)
        {
            memset(&_buf[_size], 0x5, 1);
            _size += 1;
        }
        if (std::is_same_v<T, float>)
        {
            memset(&_buf[_size], 0x6, 1);
            _size += 1;
        }
        if (std::is_same_v<T, char> || std::is_same_v<T, unsigned char> || std::is_same_v<T, bool>)
        {   
            memset(&_buf[_size], 0x4, 1);
            _size += 1;
        }
        if (std::is_same_v<T, char*> || std::is_same_v<T, unsigned char&> || std::is_same_v<T, const char*>)
        {   
            memset(&_buf[_size], 0xE, 1);
            _size += 1;
            const char * str = reinterpret_cast<const char*>(&value);
            memset(&_buf[_size], strlen(str), 1);
            _size += 1;
        }

        memcpy(&_buf[_size], &value, sizeof(value));
        _size += sizeof(value);
    }
};

class OpcodeHandler
{
private:
    struct PyCommand
    {
        std::string name = "Unknown";
        void* pfunc = nullptr;
    };

    struct PyModule 
    {
    private:
        typedef PyObject* f_cmd(PyObject* self, PyObject* args);
        typedef PyObject* f_pyvoid();

        static inline PyModule *cur_mod = nullptr;
        static PyObject* init()
        {
            size_t size = cur_mod->commands.size();
            PyMethodDef *method_def = new PyMethodDef[size]; 

            // create our method defination table
            for (size_t i = 0; i < size; ++i)
            {
                const char* name = cur_mod->commands[i].name.c_str();
                method_def[i] = {name, (f_cmd*)cur_mod->commands[i].pfunc, METH_VARARGS, NULL};
            }
            method_def[size] = {};

            // create our module defination table & create the module
            PyModuleDef *mod_def = new PyModuleDef{PyModuleDef_HEAD_INIT, cur_mod->name.c_str(), NULL, -1, method_def, NULL, NULL, NULL, NULL};
            PyObject *module = PyModule_Create(mod_def);

            return module;
        }
    public:
        std::string name = "Unknown";
        std::vector<PyCommand> commands;

        // kinda hacky, but it works
        f_pyvoid* get_init()
        {
            cur_mod = this;
            return init;
        };
    };

    static inline std::vector<PyModule> modules;

    // Calls CRunningScript on a memory buffer
    static bool call_script_on_buf(unsigned int command_id, ScriptBuffer& buf)
    {
        static CRunningScriptSA script;
        memset(&script, 0, sizeof(CRunningScriptSA));
        strcpy(script.m_szName, "py-load");
        script.m_bIsMission = false;
        script.m_bUseMissionCleanup = false;
        script.m_bNotFlag = (command_id >> 15) & 1;

        if (gGameVer == eGame::SA)
        {
            static unsigned short &commands_executed = *reinterpret_cast<unsigned short*>(0xA447F4);
            typedef char (__thiscall* opcodeTable)(CRunningScriptSA*, int);

            script.m_pBaseIP = script.m_pCurrentIP = buf.get();

            // Calling CRunningScript::ProcessOneCommand directly seems to crash
            ++commands_executed;
            script.m_pCurrentIP += 2;
            script.m_bNotFlag = (command_id & 0x8000) != 0;
            opcodeTable* f = (opcodeTable*)0x8A6168;
            f[(command_id & 0x7FFF) / 100](&script, command_id & 0x7FFF);
        }
        else
        {
            // TODO: VC & III
            script.m_bWastedBustedCheck = true;
            // script.m_nIp = reinterpret_cast<int>(code.GetData()) - reinterpret_cast<int>(CRunningScript::GetScriptSpaceBase());
        }

        return script.m_bCondResult ? true : false;
    }

    template <typename T>
    static void pack_param(ScriptBuffer& buf, T value) 
    {
        buf.add_bytes(value);
    }   

    template <typename First, typename... Rest>
    static void pack_param(ScriptBuffer& buf, First firstValue, Rest... rest) {
        pack_param(buf, firstValue);
        pack_param(buf, rest...);
    }

public:
    OpcodeHandler() = delete;
    OpcodeHandler(const OpcodeHandler&) = delete;
    
    // Adds new commands to Pyloader
    static void add_command(const char* cmd_name, const char* mod_name, void* pfunc)
    {
        for (auto &mod : modules)
        {
            // push to the module if it exists
            if (mod.name == mod_name)
            {
                PyCommand command;
                command.name = cmd_name;
                command.pfunc = pfunc;
                mod.commands.push_back(std::move(command));
                return;
            }
        }

        // otherwise create a new module and push to that
        PyModule mod;
        mod.name = mod_name;
        PyCommand command;
        command.name = cmd_name;
        command.pfunc = pfunc;
        mod.commands.push_back(std::move(command));
        modules.push_back(std::move(mod));
    }
    
    // Registers all the new commands to the interpreter
    static void register_commands()
    {
        for (auto&mod : modules)
        {
            PyImport_AppendInittab(mod.name.c_str(), mod.get_init());
        }
    }

    // call opcode using a python tuple
    static bool call(PyObject* args)
    {
        static size_t size = 0;
        size_t total_args = PyTuple_Size(args);
        unsigned short command_id = NULL;

        // return of no args provided
        if (!total_args)
        {
            Py_FatalError("No arguments provided!");
            return false;
        }

        // Error checking for opcode id
        PyObject *ptr = PyTuple_GetItem(args, 0);
        if (ptr)
        {
            if (PyNumber_Check(ptr) && !PyFloat_Check(ptr))
            {
                command_id = (unsigned short)PyLong_AsLong(PyNumber_Long(PyTuple_GetItem(args, 0)));
            }
            else
            {
                Py_FatalError("Invalid opcode id provided!");
            }
        }

        ScriptBuffer buf(command_id);
        for (size_t i = 1; i < total_args; i++)
        {
            PyObject *ptemp = PyTuple_GetItem(args, i);
            if (ptemp)
            {
                if (PyNumber_Check(ptemp))
                {
                    if (PyFloat_Check(ptemp))
                    {
                        float val = (float)PyFloat_AsDouble(PyNumber_Float(ptemp));
                        buf.add_bytes(val);
                    }
                    else
                    {
                        int val = PyLong_AsLong(PyNumber_Long(ptemp));
                        buf.add_bytes(val);
                    }
                }
                else
                {
                    char *val = PyBytes_AsString(PyUnicode_AsUTF8String(ptemp));
                    buf.add_bytes(val);
                }
            }
        }

        return call_script_on_buf(command_id, buf);
    }

    // call opcode using param pack
    template<typename... ArgTypes>
    static bool call(unsigned int command_id, ArgTypes... arguments)
    {
        ScriptBuffer buf(command_id);
        pack_param(buf, arguments...);
        return call_script_on_buf(command_id, buf);
    }
};
