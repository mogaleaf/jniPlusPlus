#pragma once
#include <jni.h>
#include <string>
#include <memory>
#include <iostream>
#include <sstream>
#include <map>
#include <algorithm>

class JavaClass {
    std::map<std::string, jmethodID> methodCache;

protected:
    std::string classPath;
    jclass classId;
    JNIEnv* env;
    
public:
    JavaClass() : JavaClass("uninitialized") {
    }

    JavaClass(std::string classpath) : classPath(classpath), classId(nullptr) {
        std::replace(this->classPath.begin(), this->classPath.end(), '.', '/');
    }

    JavaClass(std::string classPath, JNIEnv* jniEnv) : JavaClass(classPath) {
        env = jniEnv;
        classId = env->FindClass(this->classPath.c_str());
        checkExceptions("JavaClass::JavaClass FindClass");
    }

    template <typename ReturnType, typename... Args>
    ReturnType call(std::string name, const ReturnType& returnType, Args&&... args) {
        auto methodId = getStaticMethodID(name, returnType, args...);
        checkExceptions("JavaClass::call GetStaticMethodId");
        auto jvalues = createJValues(args...);
        auto result = callStaticMethod(methodId, returnType, jvalues.get());
        checkExceptions("JavaClass::call callStaticMethod");
        return result;
    }

    template <typename... Args>
    void callVoid(std::string name, Args&&... args) {
        callVoid(getStaticVoidMethodID(name, args...));
    }

    template <typename... Args>
    void callVoid(jmethodID methodId, Args&&... args) {
        checkExceptions("JavaClass::callVoid GetStaticMethodId");
        auto jvalues = createJValues(args...);
        callStaticMethodVoid(methodId, jvalues.get());
        checkExceptions("JavaClass::call callStaticMethodVoid");
    }


    template <typename... Args>
    auto createNew(std::string classPath, Args&&... args) {
        auto methodId = env->GetMethodID(classId, "<init>", voidSignature(args...).c_str());
        checkExceptions("JavaClass::createNew GetMethodId");
        auto jvalues = createJValues(args...);
        auto objId = env->NewObjectA(classId, methodId, jvalues.get());
        checkExceptions("JavaClass::createNew NewObjectA");
        return JavaObj(classPath, objId, classId, env);
    }

    template <typename... Args>
    std::unique_ptr<jvalue[]> createJValues(const Args&... args) {
        auto jvalues = std::make_unique<jvalue[]>(sizeof...(Args));
        fillJValues(jvalues.get(), 0, args...);
        return jvalues;
    }

    static std::unique_ptr<jvalue[]> createJValues() {
        return nullptr;
    }

    template <typename FuncType, typename...Args>
    void registerNativeVoid(std::string name, FuncType* function, const Args&... args) {
        auto signature = voidSignature(args...);
        std::vector<char> signatureZstr(signature.length() + 1);
        std::copy(signature.begin(), signature.end(), signatureZstr.begin());
        std::vector<char> nameZstr(name.length() + 1);
        std::copy(name.begin(), name.end(), nameZstr.begin());

        JNINativeMethod method;
        method.name = nameZstr.data();
        method.signature = signatureZstr.data();
        method.fnPtr = static_cast<void*>(function);
        if (env->RegisterNatives(classId, &method, 1) < 0) {
            std::cerr << "Cannot register native methods.\n";
            exit(EXIT_FAILURE);
        }
    }

    static void checkExceptions(std::string where, JNIEnv* env) {
        if (env->ExceptionCheck()) {
            auto throwable = env->ExceptionOccurred();
            if (throwable) {
                env->ExceptionClear();
                auto exClass(env->GetObjectClass(throwable));
                auto classClass(env->FindClass("java/lang/Class"));

                auto getNameMethod = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
                auto name = static_cast<jstring>(env->CallObjectMethod(exClass, getNameMethod));
                auto utfName = env->GetStringUTFChars(name, nullptr);

                auto getMessageMethod(env->GetMethodID(exClass, "getMessage", "()Ljava/lang/String;"));
                auto message(static_cast<jstring>(env->CallObjectMethod(throwable, getMessageMethod)));
                auto utfMessage(env->GetStringUTFChars(message, nullptr));

                std::cout << "Exception after " << where << "\n";
                std::cout << "  --> " << utfName << " : " << utfMessage << "\n";
                env->ReleaseStringUTFChars(message, utfMessage);
                env->ReleaseStringUTFChars(name, utfName);
            }
        }
    }

    template <typename ReturnType>
    ReturnType fromJObject(jobject object, const ReturnType& returnType) const {
        return ReturnType(returnType.getClassPath(), object, env);
    }

    template <typename ReturnType, typename... Args>
    jmethodID getStaticMethodID(std::string methodName, const ReturnType& returnType, Args&&... args) {
        try {
            return methodCache.at(methodName);
        }
        catch (std::out_of_range) {
            auto methodId = env->GetStaticMethodID(classId, methodName.c_str(), signature(returnType, args...).c_str());
            methodCache[methodName] = methodId;
            return methodId;
        }
    }

    template <typename... Args>
    jmethodID getStaticVoidMethodID(std::string methodName, Args&&... args) {
        try {
            return methodCache.at(methodName);
        }
        catch (std::out_of_range) {
            auto methodId = env->GetStaticMethodID(classId, methodName.c_str(), voidSignature(args...).c_str());
            methodCache[methodName] = methodId;
            return methodId;
        }
    }
    

protected:
    void checkExceptions(std::string where) const {
        checkExceptions(where, env);
    }

    template <typename Arg, typename... Args>
    static void signature(std::stringstream& os, const Arg& arg, const Args&... args) {
        os << getSymbol(arg);
        signature(os, args...);
    }

    static void signature(std::stringstream& os) {
        os << ")";
    }

    template <typename... Args>
    static std::string voidSignature(const Args&... args) {
        std::stringstream os("(", std::ios_base::out | std::ios_base::ate);
        signature(os, args...);
        os << getSymbol();
        return os.str();
    }

    template <typename ReturnType, typename... Args>
    static std::string signature(const ReturnType& returnType, const Args&... args) {
        std::stringstream os("(", std::ios_base::out | std::ios_base::ate);
        signature(os, args...);
        os << getSymbol(returnType);
        return os.str();
    }

    template <typename T>
    static std::string getSymbol(const T& type);

    static std::string getSymbol() {
        return "V";
    }

    template <typename Type>
    jvalue toJvalue(Type& obj);

private:
    template <typename Arg, typename... Args>
    void fillJValues(jvalue* jvalues, std::size_t index, const Arg& arg, const Args&... args) {
        jvalues[index] = toJvalue(arg);
        fillJValues(jvalues, index + 1, args...);
    }

    static void fillJValues(jvalue* jvalues, std::size_t) {
    }


    template <typename ReturnType>
    ReturnType callStaticMethod(jmethodID methodId, const ReturnType& returnType, jvalue* args) const {
        auto object = env->CallStaticObjectMethodA(classId, methodId, args);
        checkExceptions("JavaClass::callStaticMethod CallStaticObjectMethodA");
        return fromJObject(object, returnType);
    };

    void callStaticMethodVoid(jmethodID methodId, jvalue* args) const {
        env->CallStaticVoidMethodA(classId, methodId, args);
        checkExceptions("JavaClass::callStaticMethodVoid CallStaticVoidMethodA");
    };


};

template <>
inline std::string JavaClass::fromJObject(jobject object, const std::string&) const {
    auto javaString = static_cast<jstring>(object);
    auto zString = env->GetStringUTFChars(javaString, nullptr);
    return std::string(zString);
}


template <>
inline float JavaClass::callStaticMethod(jmethodID methodId, const float&, jvalue* args) const {
    return env->CallStaticFloatMethodA(classId, methodId, args);
}

template <>
inline jobject JavaClass::callStaticMethod(jmethodID methodId, const jobject&, jvalue* args) const {
    return env->CallStaticObjectMethodA(classId, methodId, args);
}


template <>
inline std::string JavaClass::getSymbol(const bool&) {
    return "Z";
}

template <>
inline std::string JavaClass::getSymbol(const uint8_t&) {
    return "S";
}

template <>
inline std::string JavaClass::getSymbol(const int8_t&) {
    return "C";
}

template <>
inline std::string JavaClass::getSymbol(const int16_t&) {
    return "S";
}

template <>
inline std::string JavaClass::getSymbol(const int32_t&) {
    return "I";
}

template <>
inline std::string JavaClass::getSymbol(const long&) {
    return "I";
}

template <>
inline std::string JavaClass::getSymbol(const uint16_t&) {
    return "I";
}

template <>
inline std::string JavaClass::getSymbol(const uint32_t&) {
    return "J";
}

template <>
inline std::string JavaClass::getSymbol(const int64_t&) {
    return "J";
}

template <>
inline std::string JavaClass::getSymbol(const float&) {
    return "F";
}

template <>
inline std::string JavaClass::getSymbol(const double&) {
    return "D";
}

template <>
inline std::string JavaClass::getSymbol(const std::string&) {
    return "Ljava/lang/String;";
}


template <>
inline jvalue JavaClass::toJvalue(const bool& v) {
    jvalue j;
    j.z = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const uint8_t& v) {
    jvalue j;
    j.s = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const int8_t& v) {
    jvalue j;
    j.c = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const int16_t& v) {
    jvalue j;
    j.s = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const uint16_t& v) {
    jvalue j;
    j.i = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const int32_t& v) {
    jvalue j;
    j.i = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const uint32_t& v) {
    jvalue j;
    j.j = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const int64_t& v) {
    jvalue j;
    j.j = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const float& v) {
    jvalue j;
    j.f = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const double& v) {
    jvalue j;
    j.d = v;
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const std::string& v) {
    jvalue j;
    j.l = env->NewStringUTF(v.c_str());
    return j;
}

template <>
inline jvalue JavaClass::toJvalue(const jobject& v) {
    jvalue j;
    j.l = v;
    return j;
}

