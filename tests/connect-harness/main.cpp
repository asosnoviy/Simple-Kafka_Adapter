// Connect harness: emulates the sequence of calls the 1C platform performs
// while connecting an external component.
//
//   connect_harness <path to the component>
//
// The platform calls Init and then setMemManager, and only after that the
// component allocates the first buffer. That order was verified on the 1C
// mobile platform: the memory manager it passes is its own object, so the
// component does not have to provide a fallback of its own.
//
// Exit code 0 means the whole connect sequence completed without a crash.

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#include <dlfcn.h>

#include "ComponentBase.h"
#include "AddInDefBase.h"
#include "IMemoryManager.h"

namespace {

class FakeMemoryManager : public IMemoryManager {
public:
    bool AllocMemory(void **pMemory, unsigned long ulCountByte) override {
        *pMemory = std::malloc(ulCountByte);
        return *pMemory != nullptr;
    }

    void FreeMemory(void **pMemory) override {
        std::free(*pMemory);
        *pMemory = nullptr;
    }
};

class FakePlatform : public IAddInDefBase {
public:
    bool AddError(unsigned short wcode, const WCHAR_T *source, const WCHAR_T *descr, long scode) override {
        (void)wcode;
        (void)source;
        (void)descr;
        (void)scode;
        errors++;
        return true;
    }

    bool Read(WCHAR_T *wszPropName, tVariant *pVal, long *pErrCode, WCHAR_T **errDescriptor) override {
        (void)wszPropName; (void)pVal; (void)pErrCode; (void)errDescriptor;
        return false;
    }

    bool Write(WCHAR_T *wszPropName, tVariant *pVar) override {
        (void)wszPropName; (void)pVar;
        return false;
    }

    bool RegisterProfileAs(WCHAR_T *wszProfileName) override {
        (void)wszProfileName;
        return false;
    }

    bool SetEventBufferDepth(long lDepth) override {
        (void)lDepth;
        return true;
    }

    long GetEventBufferDepth() override { return 0; }

    bool ExternalEvent(WCHAR_T *wszSource, WCHAR_T *wszMessage, WCHAR_T *wszData) override {
        (void)wszSource; (void)wszMessage; (void)wszData;
        return true;
    }

    void CleanEventBuffer() override {}

    bool SetStatusLine(WCHAR_T *wszStatusLine) override {
        (void)wszStatusLine;
        return true;
    }

    void ResetStatusLine() override {}

    long errors = 0;
};

template<typename T>
T resolve(void *handle, const char *name, bool required) {
    void *sym = dlsym(handle, name);
    if (!sym && required) {
        std::fprintf(stderr, "FAIL: symbol %s is not exported\n", name);
        std::exit(2);
    }
    return reinterpret_cast<T>(sym);
}

std::string toUtf8(const WCHAR_T *s) {
    if (!s) {
        return "(null)";
    }
    std::string out;
    for (const WCHAR_T *p = s; *p; ++p) {
        if (*p < 0x80) {
            out.push_back(static_cast<char>(*p));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <component>\n", argv[0]);
        return 2;
    }

    const char *path = argv[1];

    std::printf("component      : %s\n", path);
    std::printf("memory manager : provided by the platform\n");

    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::fprintf(stderr, "FAIL: dlopen: %s\n", dlerror());
        return 2;
    }
    std::printf("dlopen         : ok\n");

    auto getClassNames = resolve<const WCHAR_T *(*)()>(handle, "GetClassNames", true);
    auto getClassObject = resolve<long (*)(const WCHAR_T *, IComponentBase **)>(handle, "GetClassObject", true);
    auto destroyObject = resolve<long (*)(IComponentBase **)>(handle, "DestroyObject", true);
    auto setPlatformCapabilities = resolve<AppCapabilities (*)(AppCapabilities)>(handle, "SetPlatformCapabilities", true);
    auto getAttachType = resolve<AttachType (*)()>(handle, "GetAttachType", false);

    const WCHAR_T *classNames = getClassNames();
    std::printf("GetClassNames  : %s\n", toUtf8(classNames).c_str());

    std::printf("SetPlatformCapabilities : %d\n", (int)setPlatformCapabilities(eAppCapabilitiesLast));

    if (getAttachType) {
        std::printf("GetAttachType  : %d\n", (int)getAttachType());
    } else {
        std::printf("GetAttachType  : not exported\n");
    }

    IComponentBase *component = nullptr;
    long created = getClassObject(classNames, &component);
    if (!component) {
        std::fprintf(stderr, "FAIL: GetClassObject returned %ld\n", created);
        return 3;
    }
    std::printf("GetClassObject : ok\n");

    FakePlatform platform;
    FakeMemoryManager memManager;

    std::printf("Init           : %s\n", component->Init(&platform) ? "ok" : "failed");
    std::printf("setMemManager  : %s\n", component->setMemManager(&memManager) ? "ok" : "failed");

    WCHAR_T *extensionName = nullptr;
    bool registered = component->RegisterExtensionAs(&extensionName);
    std::printf("RegisterExtensionAs : %s (%s)\n", registered ? "ok" : "failed", toUtf8(extensionName).c_str());
    memManager.FreeMemory(reinterpret_cast<void **>(&extensionName));

    long props = component->GetNProps();
    std::printf("GetNProps      : %ld\n", props);
    for (long i = 0; i < props; ++i) {
        const WCHAR_T *name = component->GetPropName(i, 0);
        tVariant value{};
        component->GetPropVal(i, &value);
        component->IsPropReadable(i);
        component->IsPropWritable(i);
        if (value.vt == VTYPE_PWSTR && value.pwstrVal) {
            memManager.FreeMemory(reinterpret_cast<void **>(&value.pwstrVal));
        }
        (void)name;
    }
    std::printf("properties     : read\n");

    long methods = component->GetNMethods();
    std::printf("GetNMethods    : %ld\n", methods);
    for (long i = 0; i < methods; ++i) {
        const WCHAR_T *name = component->GetMethodName(i, 0);
        if (name) {
            // The platform frees the buffer it got, although the interface
            // hands it out as a const pointer.
            void *buffer = const_cast<void *>(static_cast<const void *>(name));
            memManager.FreeMemory(&buffer);
        }
        long params = component->GetNParams(i);
        for (long p = 0; p < params; ++p) {
            tVariant def{};
            component->GetParamDefValue(i, p, &def);
            if (def.vt == VTYPE_PWSTR && def.pwstrVal) {
                memManager.FreeMemory(reinterpret_cast<void **>(&def.pwstrVal));
            }
        }
        component->HasRetVal(i);
    }
    std::printf("methods        : read\n");

    component->Done();
    destroyObject(&component);
    dlclose(handle);

    std::printf("RESULT         : connect sequence completed\n");
    return 0;
}
