set(Title "ChacshinNoexcept")
set(Student "Chacshin_Vladimir")
set(Group "FIIT3")
set(TARGET_NAME "${Title}_${Student}_${Group}_ClangAST")

file(GLOB_RECURSE SOURCES *.cpp *.h *.hpp)
add_llvm_library(${TARGET_NAME} MODULE ${SOURCES} PLUGIN_TOOL clang)

# Добавляем префикс lib для Linux
set_target_properties(${TARGET_NAME} PROPERTIES
    PREFIX "lib"
    SUFFIX ".so"
)

message(STATUS "Processing ChacshinNoexcept plugin CMakeLists.txt")
message("NAME: ${Title}_${Student}_${Group}_ClangAST")

set(CLANG_TEST_DEPS "${TARGET_NAME}" ${CLANG_TEST_DEPS} PARENT_SCOPE)