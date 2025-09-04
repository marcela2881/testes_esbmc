#include <assert.h>
#include <stdlib.h> // Adicionado para malloc e free

// Declaração da função especial do ESBMC para gerar valores arbitrários
extern int nondet_int();

// -------------------- TESTES -------------------- //

// Teste 1: Estouro de buffer
void test_buffer_overflow() {
    int arr[5];
    int i = nondet_int();
    if (i >= 0 && i < 5) {
        arr[i] = 10; // correto
    } else {
        arr[i] = 20; // possível erro
    }
}

// Teste 2: Ponteiro nulo
void test_null_pointer() {
    int *p = 0;
    if (nondet_int()) {
        p = (int*) malloc(sizeof(int)); // Corrigido para C
    }
    assert(p != 0); // checa se nunca é nulo
    if (p) free(p); // Corrigido para C
}

// Teste 3: Divisão por zero
void test_div_zero() {
    int x = nondet_int();
    int y = nondet_int();
    if (y != 0) {
        int z = x / y;
    }
}

// Teste 4: Overflow aritmético
void test_overflow() {
    int a = nondet_int();
    int b = nondet_int();
    int c = a + b;
    assert(c >= a); // pode falhar se estourar
}

// Teste 5: Deadlock simples (simulado)
void test_deadlock() {
    int lockA = 0, lockB = 0;
    if (nondet_int()) {
        lockA = 1;
        if (!lockB) lockB = 1;
    } else {
        lockB = 1;
        if (!lockA) lockA = 1;
    }
    assert(lockA && lockB); // pode falhar se um não for adquirido
}

// Teste 6: Acesso inválido
void test_invalid_access() {
    int *p = (int*) malloc(sizeof(int)); // Corrigido para C
    free(p); // Corrigido para C
    if (nondet_int()) {
        *p = 10; // uso depois de free
    }
}

// Teste 7: Array index
void test_array_index() {
    int arr[10];
    int i = nondet_int();
    arr[i] = 123; // erro se i <0 ou >=10
}

// Teste 8: Condição corrida simulada
void test_race_condition() {
    int shared = 0;
    if (nondet_int()) {
        shared++;
    } else {
        shared--;
    }
    assert(shared == 1 || shared == -1); // pode falhar dependendo
}

// Teste 9: Inicialização
void test_uninitialized() {
    int x;
    if (nondet_int()) {
        x = 5;
    }
    assert(x == 5); // pode falhar se x não for inicializado
}

// Teste 10: Memória não liberada
void test_memory_leak() {
    int *p = (int*) malloc(sizeof(int)); // Corrigido para C
    if (nondet_int()) {
        free(p); 
    }
    // se não liberar, gera "leak"
}

// -------------------- MAIN -------------------- //

int main() {
    int escolha = nondet_int();

    if (escolha == 0) test_buffer_overflow();
    else if (escolha == 1) test_null_pointer();
    else if (escolha == 2) test_div_zero();
    else if (escolha == 3) test_overflow();
    else if (escolha == 4) test_deadlock();
    else if (escolha == 5) test_invalid_access();
    else if (escolha == 6) test_array_index();
    else if (escolha == 7) test_race_condition();
    else if (escolha == 8) test_uninitialized();
    else if (escolha == 9) test_memory_leak();

    return 0;
}