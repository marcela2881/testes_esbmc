/**
 * @file test_bmi088_imu_esbmc.cpp
 * @author Dissertação Mestrado - Verificação Formal PX4 v1.16
 * 
 * OBJETIVO: Verificação formal de funções críticas do driver BMI088 IMU
 * MÓDULO TESTADO: BMI088 Accelerometer + Gyroscope
 * MÉTODO: Bounded Model Checking com ESBMC
 */

#include <assert.h>
#include <cmath>
#include <stdint.h>

// ================== FUNÇÕES ESBMC ==================
extern int nondet_int();
extern float nondet_float();
extern uint8_t nondet_uint8();
extern uint16_t nondet_uint16();
extern void __ESBMC_assume(int condition);

// ================== CONSTANTES REAIS DO BMI088 ==================
static constexpr int32_t FIFO_MAX_SAMPLES = 32;
static constexpr size_t FIFO_SIZE = 1024;

// ================== FUNÇÕES REAIS EXTRAÍDAS DO PX4 ==================

/**
 * FUNÇÃO 1: combine() - BMI088.hpp linha ~12
 * ESPECIFICAÇÃO: Combinar bytes MSB/LSB em valor 16-bit
 */
static constexpr int16_t combine(uint8_t msb, uint8_t lsb) { 
    return (msb << 8u) | lsb; 
}

/**
 * FUNÇÃO 2: UpdateTemperature() - BMI088_Accelerometer.cpp linha ~480
 * ESPECIFICAÇÃO: Converter dados raw de temperatura para Celsius
 * FÓRMULA: Temp_int11 * 0.125°C/LSB + 23°C
 */
float updateTemperature(uint8_t temp_msb, uint8_t temp_lsb) {
    // Código REAL extraído do PX4
    uint16_t Temp_uint11 = (temp_msb * 8) + (temp_lsb / 32);
    int16_t Temp_int11 = 0;
    
    if (Temp_uint11 > 1023) {
        Temp_int11 = Temp_uint11 - 2048;
    } else {
        Temp_int11 = Temp_uint11;
    }
    
    float temperature = (Temp_int11 * 0.125f) + 23.0f;
    return temperature;
}

/**
 * FUNÇÃO 3: FIFOReadCount() - BMI088_Accelerometer.cpp linha ~340
 * ESPECIFICAÇÃO: Calcular count de bytes no FIFO (14-bit value)
 */
uint16_t fifoReadCount(uint8_t fifo_length_0, uint8_t fifo_length_1) {
    // Código REAL extraído do PX4
    const uint8_t FIFO_LENGTH_1_MASKED = fifo_length_1 & 0x3F; // fifo_byte_counter[13:8]
    return combine(FIFO_LENGTH_1_MASKED, fifo_length_0);
}

/**
 * FUNÇÃO 4: Accel Data Processing - BMI088_Accelerometer.cpp linha ~420
 * ESPECIFICAÇÃO: Processar dados do acelerômetro com flip de eixos
 */
void processAccelData(int16_t accel_y_raw, int16_t accel_z_raw, 
                      int16_t *accel_y_out, int16_t *accel_z_out) {
    // Código REAL extraído do PX4 - flip y & z para right-handed coordinate
    *accel_y_out = (accel_y_raw == INT16_MIN) ? INT16_MAX : -accel_y_raw;
    *accel_z_out = (accel_z_raw == INT16_MIN) ? INT16_MAX : -accel_z_raw;
}

/**
 * FUNÇÃO 5: Gyro Data Processing - BMI088_Gyroscope.cpp linha ~280
 * ESPECIFICAÇÃO: Processar dados do giroscópio com detecção de valores inválidos
 */
bool processGyroData(int16_t gyro_x, int16_t gyro_y, int16_t gyro_z,
                     int16_t *gyro_x_out, int16_t *gyro_y_out, int16_t *gyro_z_out) {
    // Código REAL extraído do PX4 - detectar dados inválidos
    if (gyro_x == INT16_MIN && gyro_y == INT16_MIN && gyro_z == INT16_MIN) {
        return false; // Dados inválidos
    }
    
    *gyro_x_out = gyro_x;
    *gyro_y_out = (gyro_y == INT16_MIN) ? INT16_MAX : -gyro_y;
    *gyro_z_out = (gyro_z == INT16_MIN) ? INT16_MAX : -gyro_z;
    return true;
}

// ================== TESTES DE VERIFICAÇÃO FORMAL ==================

/**
 * TESTE 1: Verificar função combine()
 * ESPECIFICAÇÃO: "Combinar MSB/LSB deve produzir valor 16-bit correto"
 */
void test_combine_function() {
    uint8_t msb = nondet_uint8();
    uint8_t lsb = nondet_uint8();
    
    int16_t result = combine(msb, lsb);
    
    // PROPRIEDADE 1: Resultado deve estar no range de int16_t
    assert(result >= INT16_MIN && result <= INT16_MAX);
    
    // PROPRIEDADE 2: MSB deve ocupar bits superiores
    assert(((result >> 8) & 0xFF) == msb);
    
    // PROPRIEDADE 3: LSB deve ocupar bits inferiores  
    assert((result & 0xFF) == lsb);
}

/**
 * TESTE 2: Verificar cálculo de temperatura
 * ESPECIFICAÇÃO: "Temperatura deve estar no range operacional do sensor"
 */
void test_temperature_calculation() {
    uint8_t temp_msb = nondet_uint8();
    uint8_t temp_lsb = nondet_uint8();
    
    float temperature = updateTemperature(temp_msb, temp_lsb);
    
    // PROPRIEDADE 1: Temperatura deve estar no range operacional (-40°C a +85°C)
    assert(temperature >= -40.0f && temperature <= 85.0f);
    
    // PROPRIEDADE 2: Resultado deve ser finito
    assert(!isnan(temperature));
    assert(!isinf(temperature));
}

/**
 * TESTE 3: Verificar FIFO count calculation  
 * ESPECIFICAÇÃO: "FIFO count deve estar dentro do tamanho máximo"
 */
void test_fifo_count_calculation() {
    uint8_t fifo_len_0 = nondet_uint8();
    uint8_t fifo_len_1 = nondet_uint8();
    
    uint16_t count = fifoReadCount(fifo_len_0, fifo_len_1);
    
    // PROPRIEDADE 1: Count não deve exceder tamanho máximo do FIFO
    assert(count <= FIFO_SIZE);
    
    // PROPRIEDADE 2: Count deve respeitar máscara de 14-bit (0x3FFF)
    assert(count <= 0x3FFF);
}

/**
 * TESTE 4: Verificar processamento de dados do acelerômetro
 * ESPECIFICAÇÃO: "Flip de eixos deve preservar magnitude e tratar INT16_MIN"
 */
void test_accel_data_processing() {
    int16_t accel_y_raw = nondet_int();
    int16_t accel_z_raw = nondet_int();
    
    __ESBMC_assume(accel_y_raw >= INT16_MIN && accel_y_raw <= INT16_MAX);
    __ESBMC_assume(accel_z_raw >= INT16_MIN && accel_z_raw <= INT16_MAX);
    
    int16_t accel_y_out, accel_z_out;
    processAccelData(accel_y_raw, accel_z_raw, &accel_y_out, &accel_z_out);
    
    // PROPRIEDADE 1: Resultados devem estar no range válido
    assert(accel_y_out >= INT16_MIN && accel_y_out <= INT16_MAX);
    assert(accel_z_out >= INT16_MIN && accel_z_out <= INT16_MAX);
    
    // PROPRIEDADE 2: Tratamento especial para INT16_MIN
    if (accel_y_raw == INT16_MIN) {
        assert(accel_y_out == INT16_MAX);
    } else {
        assert(accel_y_out == -accel_y_raw);
    }
    
    if (accel_z_raw == INT16_MIN) {
        assert(accel_z_out == INT16_MAX);
    } else {
        assert(accel_z_out == -accel_z_raw);
    }
}

/**
 * TESTE 5: Verificar processamento de dados do giroscópio
 * ESPECIFICAÇÃO: "Detectar dados inválidos e processar dados válidos"
 */
void test_gyro_data_processing() {
    int16_t gyro_x = nondet_int();
    int16_t gyro_y = nondet_int(); 
    int16_t gyro_z = nondet_int();
    
    __ESBMC_assume(gyro_x >= INT16_MIN && gyro_x <= INT16_MAX);
    __ESBMC_assume(gyro_y >= INT16_MIN && gyro_y <= INT16_MAX);
    __ESBMC_assume(gyro_z >= INT16_MIN && gyro_z <= INT16_MAX);
    
    int16_t gyro_x_out, gyro_y_out, gyro_z_out;
    bool result = processGyroData(gyro_x, gyro_y, gyro_z, 
                                  &gyro_x_out, &gyro_y_out, &gyro_z_out);
    
    // PROPRIEDADE 1: Dados inválidos devem retornar false
    if (gyro_x == INT16_MIN && gyro_y == INT16_MIN && gyro_z == INT16_MIN) {
        assert(result == false);
    } else {
        assert(result == true);
        
        // PROPRIEDADE 2: Dados válidos devem ser processados corretamente
        assert(gyro_x_out == gyro_x);
        
        if (gyro_y == INT16_MIN) {
            assert(gyro_y_out == INT16_MAX);
        } else {
            assert(gyro_y_out == -gyro_y);
        }
        
        if (gyro_z == INT16_MIN) {
            assert(gyro_z_out == INT16_MAX);
        } else {
            assert(gyro_z_out == -gyro_z);
        }
    }
}

/**
 * TESTE 6: Verificar overflow em operações aritméticas
 * ESPECIFICAÇÃO: "Operações matemáticas devem ser seguras contra overflow"
 */
void test_arithmetic_safety() {
    uint8_t temp_msb = nondet_uint8();
    uint8_t temp_lsb = nondet_uint8();
    
    // Testar operação potencialmente perigosa: temp_msb * 8
    uint16_t intermediate = temp_msb * 8;
    
    // PROPRIEDADE: Operação deve estar dentro dos limites
    assert(intermediate <= 255 * 8);
    
    // Testar cálculo completo
    uint16_t temp_uint11 = intermediate + (temp_lsb / 32);
    
    // PROPRIEDADE: Resultado deve estar no range de 11-bit
    assert(temp_uint11 <= 2047);
}

// ================== MAIN PARA ESBMC ==================
int main() {
    int test_choice = nondet_int();
    __ESBMC_assume(test_choice >= 0 && test_choice < 6);
    
    switch(test_choice) {
        case 0:
            test_combine_function();
            break;
        case 1:
            test_temperature_calculation();
            break;
        case 2:
            test_fifo_count_calculation();
            break;
        case 3:
            test_accel_data_processing();
            break;
        case 4:
            test_gyro_data_processing();
            break;
        case 5:
            test_arithmetic_safety();
            break;
    }
    
    return 0;
}

/* 
 * ================================================================
 * DOCUMENTAÇÃO
 * ================================================================
 * 
 * METODOLOGIA DEMONSTRADA:
 * 
 * 1. EXTRAÇÃO DE CÓDIGO REAL:
 *    - Funções extraídas de BMI088_Accelerometer.cpp e BMI088_Gyroscope.cpp
 *    - Sem modificações do comportamento original
 *    - Preserva lógica exata do driver PX4
 * 
 * 2. ESPECIFICAÇÕES BASEADAS NO DATASHEET BMI088:
 *    - Temperatura: range operacional -40°C a +85°C
 *    - FIFO: 14-bit counter, máximo 1024 bytes
 *    - Dados: tratamento de INT16_MIN, flip de eixos Y/Z
 * 
 * 3. PROPRIEDADES VERIFICADAS:
 *    - Corretude de operações bit-wise (combine)
 *    - Segurança aritmética (overflows)
 *    - Validação de ranges (temperatura, FIFO)
 *    - Tratamento de casos especiais (INT16_MIN)
 *    - Detecção de dados inválidos
 * 
 * 4. TÉCNICA DE VERIFICAÇÃO:
 *    - Bounded Model Checking com ESBMC
 *    - Entrada simbólica não-determinística
 *    - Assumptions para restringir domínio
 *    - Assertions para verificar propriedades críticas
 * 
 * COMANDO DE EXECUÇÃO:
 * esbmc test_bmi088_imu_esbmc.cpp --unwind 10 --overflow-check --bounds-check
 * 
 * FUNÇÕES PX4 TESTADAS:
 * - combine() [BMI088.hpp:12]
 * - UpdateTemperature() [BMI088_Accelerometer.cpp:480]
 * - FIFOReadCount() [BMI088_Accelerometer.cpp:340]
 * - Accel data processing [BMI088_Accelerometer.cpp:420]
 * - Gyro data processing [BMI088_Gyroscope.cpp:280]
 * 
 * VULNERABILIDADES INVESTIGADAS:
 * - Integer overflow em cálculos de temperatura
 * - Buffer overflow em operações FIFO
 * - Undefined behavior com INT16_MIN
 * - Arithmetic overflow em operações bit-wise
 * - Dados inválidos/corrompidos
 * 
 * ================================================================
 */
