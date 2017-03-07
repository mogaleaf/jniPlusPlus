#pragma once
#include <jni.h>
#include <jni++/JavaClass.h>

#include <string>
#include <vector>
#include <memory>
#include <map>

class JVM {
    JNIEnv* env;
    JavaVM* jvm;
    std::map<std::string, std::unique_ptr<JavaClass>> classesCache;

public:
	JVM(std::string libPath, bool verbose) {
		libPath.insert(0, "-Djava.class.path=");
		std::vector<char> zstring(libPath.length() + 1);
		std::copy(libPath.begin(), libPath.end(), zstring.begin());
        
        std::vector<JVMOption> options;
        options.emplace_back(zstring.data());
		options.emplace_back("-XX:+CreateMinidumpOnCrash");
        options.emplace_back("-Djava.compiler=NONE");
        options.emplace_back("-Xcheck:jni");
		//options.emplace_back("-Xdebug");
		//options.emplace_back("-Xrunjdwp:transport=dt_socket,server=y,suspend=n,address=5005");
		 
		if (verbose) options.emplace_back("-verbose:jni");

        JavaVMInitArgs vm_args;
        vm_args.version = JNI_VERSION_1_6;
		vm_args.nOptions = static_cast<jint>(options.size());
		vm_args.options = reinterpret_cast<JavaVMOption*>(options.data());
		vm_args.ignoreUnrecognized = false;
		if (JNI_OK != JNI_CreateJavaVM(&jvm, reinterpret_cast<void**>(&env), &vm_args)) {
			throw std::runtime_error("JVM Creation failed");
		}
	}

	~JVM() { jvm->DestroyJavaVM(); }

	template<typename T> auto CheckPointer(std::string cause, T* pointer) -> T* {
		if (pointer == nullptr) {
			env->ExceptionClear();
			throw std::runtime_error(cause + " not found");
		}
		return pointer;
	}

	JavaClass& getClass(std::string classPath) {
		try {
			return *classesCache.at(classPath).get();
		}
		catch (std::out_of_range) {
            auto cachedClass = std::make_unique<JavaClass>(classPath, env);
            auto jClassPtr = cachedClass.get();
			classesCache[classPath] = std::move(cachedClass);
			return *jClassPtr;
		}
	}

    JNIEnv* getEnv() {
        return env;
    }

private:
    struct JVMOption {
        JVMOption(const char optionString[]) { this->optionString = optionString; }
        const char* optionString;
        void *extraInfo;
    };
};

