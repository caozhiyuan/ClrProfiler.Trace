#include "config_loader.h"
#include "util.h"
#include "json.hpp"
#include "logging.h"
#include <fstream>

namespace trace
{
    using json = nlohmann::json;

    std::pair<TraceAssembly, bool> TraceAssemblyFromJson(const json::value_type& src) {
        if (!src.is_object()) {
            return std::make_pair<TraceAssembly, bool>({}, false);
        }
        const auto assemblyName = ToWSTRING(src.value("assemblyName", ""));
        const auto className = ToWSTRING(src.value("className", ""));

        if(assemblyName.empty() || className.empty()){
            return std::make_pair<TraceAssembly, bool>({}, false);
        }

        std::vector<TraceMethod> traceMethods;
        auto arr = src.value("methods", json::array());
        if (arr.is_array()) {
            for (auto& el : arr) {
                const auto methodName = ToWSTRING(el.value("methodName", ""));
                const auto paramsName = ToWSTRING(el.value("paramsName", ""));
                if (methodName.empty()) {
                    continue;
                }
                traceMethods.push_back(TraceMethod{ methodName,paramsName });
            }
        }
        if(traceMethods.empty()) {
            return std::make_pair<TraceAssembly, bool>({}, false);
        }
        return std::make_pair<TraceAssembly, bool>({ assemblyName, className, traceMethods }, true);
    }

    std::vector<TraceAssembly> LoadTraceAssembliesFromStream(std::istream& stream) {
        std::vector<TraceAssembly> traceAssemblies;
        try {
            json j;
            // parse the stream
            stream >> j;

            for (auto& el : j["instrumentation"]) {
                auto i = TraceAssemblyFromJson(el);
                if (std::get<1>(i)) {
                    traceAssemblies.push_back(std::get<0>(i));
                }
            }
        }
        catch (const json::parse_error& e) {
            Warn("Invalid TraceAssemblies: {}", e.what());
        }
        catch (const json::type_error& e) {
            Warn("Invalid TraceAssemblies: {}", e.what());
        }
        catch (...) {
            const auto ex = std::current_exception();
            try {
                if (ex) {
                    std::rethrow_exception(ex);
                }
            }
            catch (const std::exception& ex0) {
                Warn("Failed to load TraceAssemblies: {}", ex0.what());
            }
        }
        return traceAssemblies;
    }

    std::vector<TraceAssembly> LoadTraceAssemblies(const WSTRING& traceHomePath) {

        const auto traceJsonFilePath = traceHomePath + PathSeparator + "trace.json"_W;
        std::vector<TraceAssembly> traceAssemblies;
        try {
            std::ifstream stream;
            stream.open(ToString(traceJsonFilePath));
            traceAssemblies = LoadTraceAssembliesFromStream(stream);
            stream.close();
        }
        catch (...) {
            const auto ex = std::current_exception();
            try {
                if (ex) {
                    std::rethrow_exception(ex);
                }
            }
            catch (const std::exception& ex0) {
                Warn("Failed to load TraceAssemblies:{}", ex0.what());
            }
        }
        return traceAssemblies;
    }
}
