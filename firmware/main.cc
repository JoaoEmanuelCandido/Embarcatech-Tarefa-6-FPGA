extern "C" {
        #include <generated/csr.h>
        #include <irq.h>
        #include <uart.h>
}

#include <math.h>

#include "tflm/tensorflow/lite/micro/micro_interpreter.h"
#include "tflm/tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tflm/tensorflow/lite/schema/schema_generated.h"
#include "tflm/tensorflow/lite/micro/system_setup.h"
#include "model.h"

constexpr int kTensorArenaSize = 3024;
alignas(16) uint8_t tensor_arena[kTensorArenaSize];

extern "C" void isr(void) {}

void print_char(char c) {
        while(uart_txfull_read());
        uart_rxtx_write(c);
}

void print_str(const char* s){
        while(*s){
                print_char(*s++);
        }
}

void print_nl() {
        print_char('\n');
        print_char('\n');

}

void print_int(int val) {
        if (val < 0){
                print_char('-');
                val = -val;
        }

        if(val == 0){
                print_char('0');
                return;
        }

        char buf[12];
        int i = 0;
        while (val > 0){
                buf[i++] = '0' + (val % 10);
                val /= 10;
        }

        while (val > 0) {
                print_char(buf[--i]);
        }
}

void prinf_float(float val, int decimals){
        if (val < 0){
                print_char('-');
                val = -val;
        }

        int ipart = (int)val;
        print_int(ipart);
        print_char('-');

        float fpart = val - ipart;
        for (int i = 0; i < decimals; i++){
                fpart *= 10;
                int digit = (int)fpart;
                print_char('0' + digit);
                fpart -= digit;
        }
}

#ifdef CSR_CUSTOM_LEDS_OUT_ADDR
void set_leds(uint8_t value){
        (volatile uint32_t)CSR_CUSTOM_LEDS_OUT_ADDR = value;
}
#else
void set_leds(uint8_t value){
        // No LEDs
}
#endif

uint8_t sine_to_leds(float sine_value) {
        int num_leds = (int)((sine_value + 1.0f) * 4.0f + 0.5f);

        if(num_leds < 0){
                num_leds = 0;
        }
        if(num_leds > 8){
                num_leds = 8;
        }

        uint8_t pattern = 0;
        for (int i = 0; i < num_leds; i++){
                pattern |= (1 << i);
        }

        return pattern;
}

void print_leds(uint8_t pattern) {
        print_str(" [");
        for( int i = 7; i >= 0; i-- ){
                if(pattern & (1 << i)){
                        print_char('*');
                } else {
                        print_char('-');
                }
        }
        print_char(']');
}

int  main(void)
{

        uart_init();


        const tflite::Model* model = tflite::GetModel(g_model);
        if(model->version() != TFLITE_SCHEMA_VERSION){
                print_str("ERROR: Model version!");
                print_nl();
                while(1);
        }

        print_str("Criando resolvedor...");
        print_nl();

        tflite::MicroMutableOpResolver<1> resolver;
        if (resolver.AddFullyConnected() != kTfLiteOk){
                print_str("ERROR: AddFullyConnected");
                print_nl();
                while(1);
        }

        print_str("Criando Interpretador...");
        print_nl();

        tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize);

        print_str("Alocando tensors...");
        print_nl();


        if (interpreter.AllocateTensors() != kTfLiteOk){
                print_str("ERROR: AllocateTensors");
                print_nl();
                print_str("ERROR: AllocateTensors");


                while(1);
        }

        print_str("Uso da Arena:");
        print_int(interpreter.arena_used_bytes());
        print_str(" / ");
        print_int(kTensorArenaSize);
        print_nl();


        TfLiteTensor* input = interpreter.input(0);
        TfLiteTensor* output = interpreter.output(0);

        int count = 0;
        while(1){
                float x = (count * 2.0f * 3.1514f) / 200.0f;

                int8_t x_q = (int8_t)(x / input->params.scale + input->params.zero_point);
                input->data.int8[0] = x_q;

                if(interpreter.Invoke() != kTfLiteOk){
                        print_str("ERROR: Invoke");
                        print_nl();
                        while(1);

                }

                int8_t y_q = output->data.int8[0];
                float y = (y_q - output->params.zero_point) * output->params.scale;

                uint8_t led_pattern = sine_to_leds(y);


                leds_out_write((uint32_t)led_pattern);


                for (volatile int i = 0; i < 250000; i++);

                count++;

                if(count >= 200){
                        count = 0;
                        print_str("Ciclo completo.");
                        print_nl();
                }

        }


        leds_out_write(0xFF);

        print_char('h');




        return 0;
}