#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <ctime>
#include <vector>

struct Network {
    int *sizes;
    int layers;
    double **weights;
    double **biases;
    double **activations;
    double **deltas;
};

double sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }
double sigmoid_deriv(double x) { return x * (1.0 - x); }
double rand_weight() { return ((double)rand() / RAND_MAX) * 2.0 - 1.0; }

Network *create_network(int *sizes, int layers) {
    Network *n = new Network;
    n->layers = layers;
    n->sizes = new int[layers];
    memcpy(n->sizes, sizes, layers * sizeof(int));

    n->weights = new double *[layers - 1];
    n->biases = new double *[layers - 1];
    n->activations = new double *[layers];
    n->deltas = new double *[layers];

    for (int i = 0; i < layers; i++) {
        n->activations[i] = new double[sizes[i]]();
        n->deltas[i] = new double[sizes[i]]();
    }

    for (int i = 0; i < layers - 1; i++) {
        n->weights[i] = new double[sizes[i] * sizes[i + 1]];
        n->biases[i] = new double[sizes[i + 1]];
        for (int j = 0; j < sizes[i] * sizes[i + 1]; j++)
            n->weights[i][j] = rand_weight();
        for (int j = 0; j < sizes[i + 1]; j++)
            n->biases[i][j] = rand_weight();
    }

    return n;
}

void forward(Network *n, double *input) {
    memcpy(n->activations[0], input, n->sizes[0] * sizeof(double));
    for (int l = 0; l < n->layers - 1; l++) {
        int in = n->sizes[l];
        int out = n->sizes[l + 1];
        for (int j = 0; j < out; j++) {
            double sum = n->biases[l][j];
            for (int k = 0; k < in; k++)
                sum += n->activations[l][k] * n->weights[l][k * out + j];
            n->activations[l + 1][j] = sigmoid(sum);
        }
    }
}

void backward(Network *n, double *target, double lr) {
    int L = n->layers - 1;
    for (int j = 0; j < n->sizes[L]; j++) {
        double a = n->activations[L][j];
        n->deltas[L][j] = (target[j] - a) * sigmoid_deriv(a);
    }
    for (int l = L - 1; l >= 1; l--) {
        int out = n->sizes[l];
        int nxt = n->sizes[l + 1];
        for (int j = 0; j < out; j++) {
            double sum = 0;
            for (int k = 0; k < nxt; k++)
                sum += n->deltas[l + 1][k] * n->weights[l][j * nxt + k];
            n->deltas[l][j] = sum * sigmoid_deriv(n->activations[l][j]);
        }
    }
    for (int l = 0; l < L; l++) {
        int in = n->sizes[l];
        int out = n->sizes[l + 1];
        for (int j = 0; j < out; j++) {
            for (int k = 0; k < in; k++)
                n->weights[l][k * out + j] += lr * n->deltas[l + 1][j] * n->activations[l][k];
            n->biases[l][j] += lr * n->deltas[l + 1][j];
        }
    }
}

double train(Network *n, double **inputs, double **targets, int count, double lr) {
    double total_err = 0;
    for (int i = 0; i < count; i++) {
        forward(n, inputs[i]);
        backward(n, targets[i], lr);
        for (int j = 0; j < n->sizes[n->layers - 1]; j++) {
            double diff = targets[i][j] - n->activations[n->layers - 1][j];
            total_err += diff * diff;
        }
    }
    return total_err / count;
}

void save_weights(Network *n, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) { printf("Cannot save to %s\n", path); return; }
    fwrite(&n->layers, sizeof(int), 1, f);
    fwrite(n->sizes, sizeof(int), n->layers, f);
    for (int l = 0; l < n->layers - 1; l++) {
        int cnt = n->sizes[l] * n->sizes[l + 1];
        fwrite(n->weights[l], sizeof(double), cnt, f);
        fwrite(n->biases[l], sizeof(double), n->sizes[l + 1], f);
    }
    fclose(f);
    printf("Saved weights to %s\n", path);
}

Network *load_weights(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    int layers;
    fread(&layers, sizeof(int), 1, f);
    int *sizes = new int[layers];
    fread(sizes, sizeof(int), layers, f);
    Network *n = create_network(sizes, layers);
    for (int l = 0; l < n->layers - 1; l++) {
        int cnt = n->sizes[l] * n->sizes[l + 1];
        fread(n->weights[l], sizeof(double), cnt, f);
        fread(n->biases[l], sizeof(double), n->sizes[l + 1], f);
    }
    fclose(f);
    printf("Loaded weights from %s\n", path);
    return n;
}

void free_network(Network *n) {
    for (int i = 0; i < n->layers - 1; i++) {
        delete[] n->weights[i];
        delete[] n->biases[i];
    }
    for (int i = 0; i < n->layers; i++) {
        delete[] n->activations[i];
        delete[] n->deltas[i];
    }
    delete[] n->weights;
    delete[] n->biases;
    delete[] n->activations;
    delete[] n->deltas;
    delete[] n->sizes;
    delete n;
}

int main(void) {
    srand((unsigned)time(NULL));
    printf("=== Neural Network Trainer (pure C++) ===\n\n");

    const char *weights_file = "neural_weights.bin";
    int topology[] = {2, 8, 8, 1};
    int layers = 4;
    double lr = 0.5;
    int epochs = 50000;
    int data_count = 1000;

    Network *net;
    FILE *check = fopen(weights_file, "rb");
    if (check) { fclose(check); net = load_weights(weights_file); }
    else { printf("Creating new network: 2 -> 8 -> 8 -> 1\n"); net = create_network(topology, layers); }

    double **inputs = new double *[data_count];
    double **targets = new double *[data_count];
    for (int i = 0; i < data_count; i++) {
        inputs[i] = new double[2];
        targets[i] = new double[1];
        int pattern = i % 4;
        inputs[i][0] = (pattern == 2 || pattern == 3) ? 1.0 : 0.0;
        inputs[i][1] = (pattern == 1 || pattern == 3) ? 1.0 : 0.0;
        targets[i][0] = (pattern == 1 || pattern == 2) ? 1.0 : 0.0;
    }

    printf("Training %d epochs (lr=%.2f)...\n\n", epochs, lr);

    for (int e = 0; e < epochs; e++) {
        double err = train(net, inputs, targets, data_count, lr);
        if (e % 5000 == 0 || e == epochs - 1) {
            printf("Epoch %6d/%d | Loss: %.6f\n", e, epochs, err);
        }
        if (e % 10000 == 0 && e > 0) save_weights(net, weights_file);
    }

    save_weights(net, weights_file);
    printf("\nFinal: ");
    for (int i = 0; i < 4; i++) {
        forward(net, inputs[i]);
        printf("%.0f^%.0f=%.4f ", inputs[i][0], inputs[i][1], net->activations[layers-1][0]);
    }

    for (int i = 0; i < data_count; i++) { delete[] inputs[i]; delete[] targets[i]; }
    delete[] inputs; delete[] targets;
    free_network(net);
    printf("\nDone!\n");
    return 0;
}
