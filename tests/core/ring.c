// SPDX-License-Identifier: GPL-2.0-only

#undef NDEBUG
#include <gw/ring.h>
#include <assert.h>
#include <stdio.h>

static void test_nop(void)
{
	struct gw_ring_sqe *sqe;
	struct gw_ring_cqe *cqe;
	struct gw_ring ring;
	uint32_t head;
	int ret;
	int i;
	int j;

	ret = gw_ring_init(&ring, 10);
	assert(ret == 0);

	i = 0;
	while (1) {
		sqe = gw_ring_get_sqe(&ring);
		if (!sqe)
			break;
		sqe->op = GW_RING_OP_NOP;
		sqe->user_data = (uint64_t)i;
		i++;
	}
	ret = gw_ring_submit(&ring);
	assert(ret == i);
	ret = gw_ring_wait_cqe(&ring, &cqe);
	assert(ret == i);
	j = 0;
	gw_ring_for_each_cqe(&ring, head, cqe) {
		assert(cqe->res == 0);
		assert(cqe->flags == 0);
		j++;
	}
	assert(i == j);
	gw_ring_cq_advance(&ring, j);
	gw_ring_destroy(&ring);
}

/*
 * Test that the number of usable CQEs is at least twice the number of SQEs.
 */
static void test_nop_full_cqe(void)
{
	struct gw_ring_sqe *sqe;
	struct gw_ring_cqe *cqe;
	struct gw_ring ring;
	uint32_t head;
	int ret;
	int it;
	int i;
	int j;

	ret = gw_ring_init(&ring, 10);
	assert(ret == 0);

	for (it = 0; it < 2; it++) {
		i = 0;
		while (1) {
			sqe = gw_ring_get_sqe(&ring);
			if (!sqe)
				break;
			sqe->op = GW_RING_OP_NOP;
			sqe->user_data = (uint64_t)i;
			i++;
		}
		ret = gw_ring_submit(&ring);
		assert(ret == i);
	}

	ret = gw_ring_wait_cqe(&ring, &cqe);
	assert(ret == i*2);
	j = 0;
	gw_ring_for_each_cqe(&ring, head, cqe) {
		assert(cqe->res == 0);
		assert(cqe->flags == 0);
		j++;
	}
	assert(i*2 == j);
	gw_ring_cq_advance(&ring, j);
	gw_ring_destroy(&ring);
}

int main(void)
{
	test_nop();
	test_nop_full_cqe();
	return 0;
}
