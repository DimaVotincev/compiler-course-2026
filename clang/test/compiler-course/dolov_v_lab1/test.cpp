// RUN: %clang_cc1 -load %llvmshlibdir/OverrideCheckPlugin_Dolov_Vyacheslav_FIIT3_ClangAST%pluginext \
// RUN: -plugin dolov_v_lab1 -Wno-inconsistent-missing-override -fsyntax-only -verify %s

class Base {
public:
    virtual void simpleFunc(); // expected-note {{overridden virtual function is here}}
    virtual void alreadyCorrect();
    virtual ~Base() = default; // expected-note {{overridden virtual function is here}}
};

class Derived : public Base {
public:
    void simpleFunc(); // expected-warning {{method 'simpleFunc' overrides a virtual function but is missing the 'override' specifier}}

    ~Derived(); // expected-warning {{method '~Derived' overrides a virtual function but is missing the 'override' specifier}}

    void alreadyCorrect() override; 
};

class SecondBase {
public:
    virtual void deepFunc();
};

class Middle : public SecondBase {
public:
    void deepFunc() override; // expected-note {{overridden virtual function is here}}
};

class FinalChild : public Middle {
public:
    void deepFunc(); // expected-warning {{method 'deepFunc' overrides a virtual function but is missing the 'override' specifier}}
};

class NoWarnings {
public:
    void normalMethod(); 
    static void staticMethod(); 
};

class ChildOfNoWarnings : public NoWarnings {
public:
    void normalMethod(); 
};