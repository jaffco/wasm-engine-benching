extern "C" {    

void process(float* input, float* output, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        output[i] = input[i] * 0.2f;
    }
}

}