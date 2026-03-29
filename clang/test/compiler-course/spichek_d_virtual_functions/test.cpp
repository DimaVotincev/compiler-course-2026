// RUN: %clang_cc1 -load %llvmshlibdir/spichek_d_virtual_functions_ClangAST%pluginext -plugin missing_override_plugin -fsyntax-only -Wno-everything -verify %s
class Base {
public:
    virtual void doSomething();
    virtual void doAnotherThing();
    virtual void doVirtualThing();
    virtual ~Base();
};

class Derived : public Base {
public:
    // Случай 1: Отсутствует override у обычного переопределения (должен быть warning)
    // expected-warning@+1 {{virtual function overrides a base class virtual function but is not marked with 'override'}}
    void doSomething();

    // Случай 2: Правильное переопределение с override (warning нет)
    void doAnotherThing() override;

    // Случай 3: Указано virtual, но нет override (должен быть warning)
    // Указание virtual не заменяет необходимость писать override
    // expected-warning@+1 {{virtual function overrides a base class virtual function but is not marked with 'override'}}
    virtual void doVirtualThing();

    // Случай 4: Деструктор переопределяет виртуальный деструктор базы без override (должен быть warning)
    // expected-warning@+1 {{virtual function overrides a base class virtual function but is not marked with 'override'}}
    ~Derived();

    // Случай 5: Совершенно новая виртуальная функция (warning нет)
    virtual void newVirtualFunction();

    // Случай 6: Обычная невиртуальная функция (warning нет)
    void normalFunction();
};

class DeepDerived : public Derived {
public:
    // Случай 7: Глубокое наследование, пропущен override (должен быть warning)
    // expected-warning@+1 {{virtual function overrides a base class virtual function but is not marked with 'override'}}
    void doSomething();
    
    // Случай 8: Переопределение новой виртуальной функции из Derived (должен быть warning)
    // expected-warning@+1 {{virtual function overrides a base class virtual function but is not marked with 'override'}}
    void newVirtualFunction();

    // Случай 9: Деструктор с override (warning нет)
    ~DeepDerived() override;
};