#include <pif_plugin.h>
#include <pif_common.h>

#include <stdint.h>
#include <nfp/me.h>
#include <nfp.h>
#include <nfp/mem_atomic.h>

#define CPU_MAX 10
#define FLOWS 5000
#define NUM_CENTROIDS 2
#define INTERVAL 550
#define MAXUINT 4294967295
#define AMORTIZATION 20

__declspec(emem shared scope(global) export) uint32_t hash_table_bytes[FLOWS];
__declspec(emem shared scope(global) export) uint16_t hash_table_counter[FLOWS];
__declspec(emem shared scope(global) export) uint32_t hash_table_labels[FLOWS];
__declspec(emem shared scope(global) export) uint32_t hash_table_ts[FLOWS][2];
__declspec(emem export aligned(64)) int global_semaforos[CPU_MAX + 1];
__declspec(emem shared scope(global) export) uint32_t hash_table_centroids[FLOWS];

__declspec(emem shared scope(global) export) uint32_t label;
__declspec(emem shared scope(global) export) uint32_t good_centroid_index;
__declspec(emem shared scope(global) export) uint32_t amrt_counter;
__declspec(emem shared scope(global) export) uint32_t count_centroids;
__declspec(emem shared scope(global) export) uint32_t centroids[NUM_CENTROIDS + 1][7];

typedef struct
{
    uint32_t x;
    uint32_t y;
} pos;

void semaforo_down(volatile __declspec(mem addr40) void *addr)
{
    /* semaforo "DOWN" = claim = wait */
    unsigned int addr_hi, addr_lo;
    __declspec(read_write_reg) int xfer;
    SIGNAL_PAIR my_signal_pair;

    addr_hi = ((unsigned long long int)addr >> 8) & 0xff000000;
    addr_lo = (unsigned long long int)addr & 0xffffffff;

    do
    {
        xfer = 1;
        __asm {
       mem[test_subsat, xfer, addr_hi, <<8, addr_lo, 1], \
         sig_done[my_signal_pair];
       ctx_arb[my_signal_pair]
        }
    } while (xfer == 0);
}

void semaforo_up(volatile __declspec(mem addr40) void *addr)
{
    /* semaforo "UP" = release = signal */
    unsigned int addr_hi, addr_lo;
    __declspec(read_write_reg) int xfer;
    addr_hi = ((unsigned long long int)addr >> 8) & 0xff000000;
    addr_lo = (unsigned long long int)addr & 0xffffffff;

    __asm {
    mem[incr, --, addr_hi, <<8, addr_lo, 1];
    }
}

void pif_plugin_init()
{
    //
}

void pif_plugin_init_master()
{
    uint32_t i;
    uint32_t j;
    for (i = 0; i < CPU_MAX + 1; i++)
    {
        global_semaforos[i] = 0;
    }
    for (i = 0; i < FLOWS; i++)
    {

        hash_table_centroids[i] = 0;
        hash_table_counter[i] = 0;
        hash_table_bytes[i] = 0;
        for (j = 0; j < 2; j++)
            hash_table_ts[i][j] = 0;

        hash_table_labels[i] = 0;
    }

    for (i = 0; i < NUM_CENTROIDS + 1; i++)
    {
        for (j = 0; j < 7; j++)
        {
            centroids[i][j] = 0;
        }
    }
    label = 0;
    good_centroid_index = 0;
    count_centroids = 0;
    amrt_counter = 1000;
}

int getHash(uint32_t ip1, uint32_t ip2, uint32_t HASH_MAX)
{
    uint32_t hash_key[3];
    uint32_t hash_id;
    hash_key[0] = ip1;
    hash_key[1] = ip2;
    hash_id = hash_me_crc32((void *)hash_key, sizeof(hash_key), 1);
    hash_id &= HASH_MAX;
    return (int)hash_id;
}

uint32_t getDistance(pos point, uint32_t aux, uint32_t aux2)
{
    uint32_t distance;
    distance = (aux - point.x) + (aux2 - point.y);
    if (point.x + point.y > aux + aux2)
        distance = MAXUINT - distance;

    return distance;
}

uint32_t calculate_distance(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
    uint32_t distance;
    distance = (x1 - x2) + (y1 - y2);
    if (x1 + y1 > x2 + y2)
        distance = MAXUINT - distance;

    return distance;
}

uint8_t find_centroid_index_from_color(uint32_t color)
{
    __xread uint32_t xr;
    uint32_t aux = 0;
    uint8_t i = 0;

    for (i = 0; i < NUM_CENTROIDS; i++)
    {
        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][2], sizeof(xr));
        aux = xr;

        if (aux == color)
        {
            return i;
        }
    }

    return i;
}

uint32_t find_closest_centroid(pos point)
{
    __xwrite uint32_t xw = 0;
    __xread uint32_t xr;
    uint32_t aux = 0, aux2 = 0;
    uint32_t distance_i;
    uint32_t distance;
    uint8_t index = 0;
    uint8_t i;

    mem_read_atomic(&xr, (__mem40 void *)&centroids[0][0], sizeof(xr));
    aux = xr;
    mem_read_atomic(&xr, (__mem40 void *)&centroids[0][1], sizeof(xr));
    aux2 = xr;
    distance = getDistance(point, aux, aux2);

    for (i = 1; i < NUM_CENTROIDS + 1; i++)
    {
        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][0], sizeof(xr));
        aux = xr;
        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][1], sizeof(xr));
        aux2 = xr;
        distance_i = getDistance(point, aux, aux2);
        if (distance_i < distance)
        {
            distance = distance_i;
            index = i;
        }
    }

    mem_read_atomic(&xr, (__mem40 void *)&centroids[index][2], sizeof(xr));
    aux = xr;
    return aux;
}

void decide_centroid()
{
    __xwrite uint32_t xw = 0;
    __xread uint32_t xr;
    uint32_t aux1 = 0, aux2 = 0, aux3 = 0, aux4 = 0;
    uint32_t local_good_centroid_index = 0;
    uint32_t less_active = 0;
    uint32_t largest_ipg = 0;
    uint32_t good_index = 0;
    uint8_t la_index = 0;
    uint32_t smallest_neighbor = 0;
    uint8_t sn_index = 0;
    uint8_t i;
    uint32_t distance = 0;
    mem_read_atomic(&xr, (__mem40 void *)&centroids[0][5], sizeof(xr));
    less_active = xr;
    mem_read_atomic(&xr, (__mem40 void *)&centroids[0][4], sizeof(xr));
    smallest_neighbor = xr;

    for (i = 1; i < NUM_CENTROIDS + 1; i++)
    {
        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][5], sizeof(xr));
        aux1 = xr;
        if (aux1 < less_active)
        {
            less_active = aux1;
            la_index = i;
        }
        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][4], sizeof(xr));
        aux2 = xr;
        if (i != NUM_CENTROIDS && aux2 < smallest_neighbor)
        {
            smallest_neighbor = aux2;
            sn_index = i;
        }
    }

    if (la_index == NUM_CENTROIDS)
        return;

    mem_read_atomic(&xr, (__mem40 void *)&good_centroid_index, sizeof(xr));
    local_good_centroid_index = xr;

    if (la_index != local_good_centroid_index)
    {

        mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][1], sizeof(xr));
        aux1 = xr;
        aux2 = (la_index + 1) % 2;
        mem_read_atomic(&xr, (__mem40 void *)&centroids[aux2][1], sizeof(xr));
        aux3 = xr;
        if (aux1 > aux3)
        {
            mem_read_atomic(&xr, (__mem40 void *)&centroids[local_good_centroid_index][0], sizeof(xr));
            aux1 = xr;
            xw = aux1;
            mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][0], sizeof(xw));
            mem_read_atomic(&xr, (__mem40 void *)&centroids[local_good_centroid_index][1], sizeof(xr));
            aux1 = xr;
            xw = aux1;
            mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][1], sizeof(xw));
            mem_read_atomic(&xr, (__mem40 void *)&centroids[local_good_centroid_index][2], sizeof(xr));
            aux1 = xr;
            xw = aux1;
            mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][2], sizeof(xw));
            mem_read_atomic(&xr, (__mem40 void *)&centroids[local_good_centroid_index][3], sizeof(xr));
            aux1 = xr;
            xw = aux1;
            mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][3], sizeof(xw));
            mem_read_atomic(&xr, (__mem40 void *)&centroids[local_good_centroid_index][4], sizeof(xr));
            aux1 = xr;
            xw = aux1;
            mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][4], sizeof(xw));
            mem_read_atomic(&xr, (__mem40 void *)&centroids[local_good_centroid_index][5], sizeof(xr));
            aux1 = xr;
            xw = aux1;
            mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][5], sizeof(xw));
            mem_read_atomic(&xr, (__mem40 void *)&centroids[local_good_centroid_index][6], sizeof(xr));
            aux1 = xr;
            xw = aux1;
            mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][6], sizeof(xw));
        }
    }
    else if (la_index == local_good_centroid_index)
    {
        mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][1], sizeof(xr));
        aux1 = xr;
        aux2 = (la_index + 1) % 2;
        mem_read_atomic(&xr, (__mem40 void *)&centroids[aux2][1], sizeof(xr));
        aux3 = xr;
        if (aux1 < aux3)
            la_index = aux2;
    }

    mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][0], sizeof(xr));
    aux1 = xr;
    xw = aux1;
    mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][0], sizeof(xw));
    mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][1], sizeof(xr));
    aux1 = xr;
    xw = aux1;
    mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][1], sizeof(xw));
    mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][2], sizeof(xr));
    aux1 = xr;
    xw = aux1;
    mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][2], sizeof(xw));
    mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][3], sizeof(xr));
    aux1 = xr;
    xw = aux1;
    mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][3], sizeof(xw));
    mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][4], sizeof(xr));
    aux1 = xr;
    xw = aux1;
    mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][4], sizeof(xw));
    mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][5], sizeof(xr));
    aux1 = xr;
    xw = aux1;
    mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][5], sizeof(xw));
    mem_read_atomic(&xr, (__mem40 void *)&centroids[NUM_CENTROIDS][6], sizeof(xr));
    aux1 = xr;
    xw = aux1;
    mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][6], sizeof(xw));

    for (i = 0; i < NUM_CENTROIDS; i++)
    {
        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][1], sizeof(xr));
        aux1 = xr;
        if (aux1 > largest_ipg)
        {
            largest_ipg = aux1;
            good_index = i;
        }

        if (la_index != i)
        {
            mem_read_atomic(&xr, (__mem40 void *)&centroids[la_index][0], sizeof(xr));
            aux1 = xr;
            mem_read_atomic(&xr, (__mem40 void *)&centroids[la_index][1], sizeof(xr));
            aux2 = xr;
            mem_read_atomic(&xr, (__mem40 void *)&centroids[i][0], sizeof(xr));
            aux3 = xr;
            mem_read_atomic(&xr, (__mem40 void *)&centroids[i][1], sizeof(xr));
            aux4 = xr;

            distance = calculate_distance(aux1, aux2, aux3, aux4) / 3;

            mem_read_atomic(&xr, (__mem40 void *)&centroids[la_index][3], sizeof(xr));
            aux1 = xr;
            mem_read_atomic(&xr, (__mem40 void *)&centroids[i][3], sizeof(xr));
            aux2 = xr;

            xw = distance;

            if (distance < aux1 || aux1 == 0)
            {
                mem_write_atomic(&xw, (__mem40 void *)&centroids[la_index][3], sizeof(xw));
            }
            if (distance < aux2 || aux2 == 0)
            {
                mem_write_atomic(&xw, (__mem40 void *)&centroids[i][3], sizeof(xw));
            }
        }
    }
    xw = good_index;
    mem_write_atomic(&xw, (__mem40 void *)&good_centroid_index, sizeof(xw));

    mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][4], sizeof(xw));
    mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][5], sizeof(xw));
}

int pif_plugin_do_clustering(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *match_data)
{
    pos point;
    PIF_PLUGIN_spinner_T *spinner = pif_plugin_hdr_get_spinner(headers);
    PIF_PLUGIN_ipv4_T *ipv4 = pif_plugin_hdr_get_ipv4(headers);
    uint32_t color = 0;
    uint32_t is_assigned = 0;
    uint32_t old_color = 0;
    uint32_t cur_count = 0;
    uint32_t update_point = 0;
    uint32_t aux, aux2, aux3;
    uint32_t total_bytes = 0;
    uint32_t flow_id;
    uint32_t data_length = 0;
    uint32_t current_timestamp;
    uint32_t old_timestamp;
    uint32_t local_count_centroids;
    uint32_t i, j, distances[NUM_CENTROIDS];
    uint8_t is_outside_clusters = 1;
    __xwrite uint32_t xw = 0;
    __xread uint32_t xr;

    flow_id = getHash(ipv4->srcAddr, ipv4->dstAddr, FLOWS - 1);

    mem_read_atomic(&xr, (__mem40 void *)&count_centroids, sizeof(xr));
    local_count_centroids = xr;

    data_length = ipv4->totalLen;

    mem_read_atomic(&xr, (__mem40 void *)&good_centroid_index, sizeof(xr));
    aux2 = xr;

    mem_read_atomic(&xr, (__mem40 void *)&hash_table_centroids[flow_id], sizeof(xr));
    aux3 = xr;

    PIF_HEADER_SET_spinner___v9(spinner, (uint32_t)aux2);
    PIF_HEADER_SET_spinner___v10(spinner, (uint32_t)aux3);

    if (aux2 == aux3)
    {
        PIF_HEADER_SET_ipv4___ecn(ipv4, 1);
    }

    semaforo_up(&global_semaforos[0]);

    mem_read_atomic(&xr, (__mem40 void *)&hash_table_counter[flow_id], sizeof(xr));
    aux = xr;

    if (aux == 0)
    {
        xw = 1;
        mem_write_atomic(&xw, (__mem40 void *)&hash_table_counter[flow_id], sizeof(xw));
        xw = data_length;
        mem_write_atomic(&xw, (__mem40 void *)&hash_table_bytes[flow_id], sizeof(xw));
        xw = spinner->v1;
        mem_write_atomic(&xw, (__mem40 void *)&hash_table_ts[flow_id][0], sizeof(xw));
        xw = spinner->v2;
        mem_write_atomic(&xw, (__mem40 void *)&hash_table_ts[flow_id][1], sizeof(xw));
    }
    else
    {
        mem_incr32((__mem40 void *)&hash_table_counter[flow_id]);
        xw = data_length;
        mem_add32(&xw, (__mem40 void *)&hash_table_bytes[flow_id], sizeof(xw));
        mem_read_atomic(&xr, (__mem40 void *)&hash_table_counter[flow_id], sizeof(xr));
        cur_count = xr;

        if (cur_count >= INTERVAL)
        {
            xw = 0;
            mem_write_atomic(&xw, (__mem40 void *)&hash_table_counter[flow_id], sizeof(xw));
            update_point = 1;                                                                
        }
    }

    mem_read_atomic(&xr, (__mem40 void *)&hash_table_bytes[flow_id], sizeof(xr));
    total_bytes = xr;

    if (update_point)
    {
        current_timestamp = spinner->v2;
        mem_read_atomic(&xr, (__mem40 void *)&hash_table_labels[flow_id], sizeof(xr));
        old_color = xr;

        mem_read_atomic(&xr, (__mem40 void *)&hash_table_ts[flow_id][1], sizeof(xr));
        aux = xr;
        old_timestamp = aux;

        point.x = ((total_bytes) / cur_count);

        point.y = (total_bytes * 8 * 10000000) / ((current_timestamp - old_timestamp) / 10);

        for (i = 0; i < NUM_CENTROIDS; i++)
        {
            mem_read_atomic(&xr, (__mem40 void *)&centroids[i][0], sizeof(xr));
            aux = xr;
            mem_read_atomic(&xr, (__mem40 void *)&centroids[i][1], sizeof(xr));
            aux2 = xr;

            if (getDistance(point, aux, aux2) <= centroids[i][3])
            {
                is_outside_clusters = 0;
                break;
            }
        }

        mem_read_atomic(&xr, (__mem40 void *)&amrt_counter, sizeof(xr));
        aux = xr;
        if (aux < AMORTIZATION)
        {
            for (i = 0; i < NUM_CENTROIDS; i++)
            {
                mem_read_atomic(&xr, (__mem40 void *)&centroids[i][0], sizeof(xr));
                aux = xr;
                mem_read_atomic(&xr, (__mem40 void *)&centroids[i][1], sizeof(xr));
                aux2 = xr;

                distances[i] = getDistance(point, aux, aux2);
                if (distances[i] <= centroids[i][3])
                {
                    mem_read_atomic(&xr, (__mem40 void *)&centroids[i][2], sizeof(xr));
                    aux = xr;
                    xw = aux;
                    mem_write_atomic(&xw, (__mem40 void *)&hash_table_labels[flow_id], sizeof(xw));
                    is_assigned = 1;
                    break;
                }
            }

            if (is_assigned == 0)
            {
                color = find_closest_centroid(point);
                xw = color;
                mem_write_atomic(&xw, (__mem40 void *)&hash_table_labels[flow_id], sizeof(xw));
            }
            aux3 = find_centroid_index_from_color(color);
            mem_incr32((__mem40 void *)&centroids[aux3][5]);


            mem_incr32((__mem40 void *)&amrt_counter);
        }
        else if (aux == AMORTIZATION)
        {
            decide_centroid();
            mem_incr32((__mem40 void *)&amrt_counter);

        }
        else if (is_outside_clusters)
        {
            if (local_count_centroids < NUM_CENTROIDS)
            {
                xw = point.x;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[local_count_centroids][0], sizeof(xw));
                xw = point.y;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[local_count_centroids][1], sizeof(xw));
                mem_read_atomic(&xr, (__mem40 void *)&label, sizeof(xr));
                aux = xr;
                xw = aux;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[local_count_centroids][2], sizeof(xw));
                mem_write_atomic(&xw, (__mem40 void *)&hash_table_labels[flow_id], sizeof(xw));
                xw = 1;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[local_count_centroids][4], sizeof(xw));

                if (local_count_centroids > 0)
                {
                    for (i = 0; i < local_count_centroids; i++)
                    {
                        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][3], sizeof(xr));
                        aux3 = xr;
                        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][0], sizeof(xr));
                        aux = xr;
                        mem_read_atomic(&xr, (__mem40 void *)&centroids[i][1], sizeof(xr));
                        aux2 = xr;
                        distances[0] = getDistance(point, aux, aux2) / 3;

                        xw = distances[0];
                        mem_write_atomic(&xw, (__mem40 void *)&centroids[local_count_centroids][3], sizeof(xw));
                        if (distances[0] < aux3 || aux3 == 0)
                        {
                            mem_write_atomic(&xw, (__mem40 void *)&centroids[i][3], sizeof(xw));
                        }
                    }
                }
                mem_incr32((__mem40 void *)&count_centroids);
                mem_incr32((__mem40 void *)&label);
            }
            else
            {
                xw = point.x;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][0], sizeof(xw));
                xw = point.y;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][1], sizeof(xw));
                mem_read_atomic(&xr, (__mem40 void *)&label, sizeof(xr));
                aux = xr;
                xw = aux;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][2], sizeof(xw));
                mem_write_atomic(&xw, (__mem40 void *)&hash_table_labels[flow_id], sizeof(xw));
                xw = 0;
                for (i = 0; i < NUM_CENTROIDS; i++)
                {
                    mem_write_atomic(&xw, (__mem40 void *)&centroids[i][5], sizeof(xw));
                }
                mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][3], sizeof(xw));
                mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][5], sizeof(xw));
                mem_write_atomic(&xw, (__mem40 void *)&amrt_counter, sizeof(xw));
                xw = 1;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][4], sizeof(xw));
                mem_incr32((__mem40 void *)&label);
                xw = flow_id;
                mem_write_atomic(&xw, (__mem40 void *)&centroids[NUM_CENTROIDS][6], sizeof(xw));
            }

            if (ipv4->ecn == 3)
            {
                PIF_HEADER_SET_ipv4___ecn(ipv4, 1);
            }
        }
        else
        {
            for (i = 0; i < NUM_CENTROIDS; i++)
            {
                mem_read_atomic(&xr, (__mem40 void *)&centroids[i][0], sizeof(xr));
                aux = xr;
                mem_read_atomic(&xr, (__mem40 void *)&centroids[i][1], sizeof(xr));
                aux2 = xr;

                distances[i] = getDistance(point, aux, aux2);
                if (distances[i] <= centroids[i][3])
                {
                    mem_read_atomic(&xr, (__mem40 void *)&centroids[i][2], sizeof(xr));
                    aux = xr;
                    xw = aux;
                    mem_write_atomic(&xw, (__mem40 void *)&hash_table_labels[flow_id], sizeof(xw));
                    xw = i;
                    mem_write_atomic(&xw, (__mem40 void *)&hash_table_centroids[flow_id], sizeof(xw));

                    mem_read_atomic(&xr, (__mem40 void *)&good_centroid_index, sizeof(xr));
                    aux2 = xr;
                    if (aux2 == i)
                    {
                        PIF_HEADER_SET_ipv4___ecn(ipv4, 1);
                    }
                    break;
                }
            }
        }

        mem_read_atomic(&xr, (__mem40 void *)&hash_table_labels[flow_id], sizeof(xr));
        aux = xr;
        if (old_color != hash_table_labels[flow_id])
        {
            aux3 = find_centroid_index_from_color(aux);
            mem_incr32((__mem40 void *)&centroids[aux3][4]);
            aux3 = find_centroid_index_from_color(old_color);
            mem_decr32((__mem40 void *)&centroids[aux3][4]);
        }
    }

    semaforo_down(&global_semaforos[0]);


    return PIF_PLUGIN_RETURN_FORWARD;
}
