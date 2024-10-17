#include <arpa/inet.h>
#include <unistd.h>
#include <malloc.h>

#include "sock.h"
#include "ib.h"
#include "debug.h"
#include "config.h"
#include "setup_ib.h"

struct IBRes ib_res;

int connect_qp_server ()
{
    int			ret	      = 0, n = 0;
    int			sockfd	      = 0;
    int			peer_sockfd   = 0;
    struct sockaddr_in	peer_addr;
    socklen_t		peer_addr_len = sizeof(struct sockaddr_in);
    char sock_buf[64]		      = {'\0'};
    struct QPInfo	local_qp_info, remote_qp_info;

    printf("Creating server socket...\n");
    sockfd = sock_create_bind(config_info.sock_port);
    check(sockfd > 0, "Failed to create server socket.");
    printf("Server socket created and bound to port %s.\n", config_info.sock_port);

    printf("Listening on server socket...\n");
    listen(sockfd, 5);

    printf("Accepting connection from client...\n");
    peer_sockfd = accept(sockfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    check (peer_sockfd > 0, "Failed to create peer_sockfd");
    printf("Connection accepted from client.\n");

    /* init local qp_info */
    local_qp_info.lid	 = ib_res.port_attr.lid; 
    local_qp_info.qp_num = ib_res.qp->qp_num;
    printf("Initialized local QP info: LID = %d, QP number = %u.\n", local_qp_info.lid, local_qp_info.qp_num);
    
    /* get qp_info from client */
    printf("Receiving QP info from client...\n");
    ret = sock_get_qp_info (peer_sockfd, &remote_qp_info);
    check (ret == 0, "Failed to get qp_info from client");
    printf("Received QP info from client: LID = %d, QP number = %u.\n", remote_qp_info.lid, remote_qp_info.qp_num);
    
    /* send qp_info to client */    
    printf("Sending QP info to client...\n");
    ret = sock_set_qp_info (peer_sockfd, &local_qp_info);
    check (ret == 0, "Failed to send qp_info to client");
    printf("Sent QP info to client.\n");

    /* change send QP state to RTS */    	
    printf("Modifying QP state to RTS...\n");
    ret = modify_qp_to_rts (ib_res.qp, remote_qp_info.qp_num, remote_qp_info.lid);
    check (ret == 0, "Failed to modify qp to rts");
    printf("QP state modified to RTS.\n");

    log (LOG_SUB_HEADER, "Start of IB Config");
    log ("\tqp[%"PRIu32"] <-> qp[%"PRIu32"]", ib_res.qp->qp_num, remote_qp_info.qp_num);
    log (LOG_SUB_HEADER, "End of IB Config");

    /* sync with clients */
    printf("Synchronizing with client...\n");
    n = sock_read (peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    check (n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");
    printf("Received sync from client.\n");
    
    n = sock_write (peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    check (n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");
    printf("Sent sync to client.\n");
    
    close (peer_sockfd);
    close (sockfd);
    printf("Closed peer and server sockets.\n");
    
    return 0;

 error:
    if (peer_sockfd > 0) {
    close (peer_sockfd);
    }
    if (sockfd > 0) {
    close (sockfd);
    }
    
    return -1;
}

int connect_qp_client ()
{
    int ret	      = 0, n = 0;
    int peer_sockfd   = 0;
    char sock_buf[64] = {'\0'};

    struct QPInfo local_qp_info, remote_qp_info;

    printf("Creating client socket and connecting to server...\n");
    peer_sockfd = sock_create_connect (config_info.server_name, config_info.sock_port);
    check (peer_sockfd > 0, "Failed to create peer_sockfd");
    printf("Client socket created and connected to server at %s:%s.\n", config_info.server_name, config_info.sock_port);

    local_qp_info.lid     = ib_res.port_attr.lid; 
    local_qp_info.qp_num  = ib_res.qp->qp_num; 
    printf("Initialized local QP info: LID = %d, QP number = %u.\n", local_qp_info.lid, local_qp_info.qp_num);
   
    /* send qp_info to server */    
    printf("Sending QP info to server...\n");
    ret = sock_set_qp_info (peer_sockfd, &local_qp_info);
    check (ret == 0, "Failed to send qp_info to server");
    printf("Sent QP info to server.\n");

    /* get qp_info from server */    
    printf("Receiving QP info from server...\n");
    ret = sock_get_qp_info (peer_sockfd, &remote_qp_info);
    check (ret == 0, "Failed to get qp_info from server");
    printf("Received QP info from server: LID = %d, QP number = %u.\n", remote_qp_info.lid, remote_qp_info.qp_num);

    /* change QP state to RTS */    	
    printf("Modifying QP state to RTS...\n");
    ret = modify_qp_to_rts (ib_res.qp, remote_qp_info.qp_num, remote_qp_info.lid);
    check (ret == 0, "Failed to modify qp to rts");
    printf("QP state modified to RTS.\n");

    log (LOG_SUB_HEADER, "IB Config");
    log ("\tqp[%"PRIu32"] <-> qp[%"PRIu32"]", ib_res.qp->qp_num, remote_qp_info.qp_num);
    log (LOG_SUB_HEADER, "End of IB Config");

    /* sync with server */
    printf("Synchronizing with server...\n");
    n = sock_write (peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    check (n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");
    printf("Sent sync to server.\n");
    
    n = sock_read (peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    check (n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");
    printf("Received sync from server.\n");

    close (peer_sockfd);
    printf("Closed client socket.\n");
    return 0;

 error:
    if (peer_sockfd > 0) {
    close (peer_sockfd);
    }
    
    return -1;
}

int setup_ib ()
{
    int	ret		         = 0;
    struct ibv_device **dev_list = NULL;    
    memset (&ib_res, 0, sizeof(struct IBRes));
    
    /* get IB device list */
    dev_list = ibv_get_device_list(NULL);
    check(dev_list != NULL, "Failed to get ib device list.");
    printf("IB Device List:\n");
    for (int i = 0; dev_list[i] != NULL; ++i) {
        printf("  Device %d: %s\n", i, ibv_get_device_name(dev_list[i]));
    }

    /* create IB context */
    ib_res.ctx = ibv_open_device(dev_list[2]);
    check(ib_res.ctx != NULL, "Failed to open ib device.");
    printf("IB Context Info:\n");
    printf("  Device Name: %s\n", ibv_get_device_name(ib_res.ctx->device));
    printf("  Device Path: %s\n", ib_res.ctx->device->dev_path);
    printf("  IB Device Name: %s\n", ib_res.ctx->device->name);
    // Removed invalid field reference: node_guid
    // Removed invalid field reference: vendor_id
    // Removed invalid field reference: vendor_part_id

    /* allocate protection domain */
    ib_res.pd = ibv_alloc_pd(ib_res.ctx);
    check(ib_res.pd != NULL, "Failed to allocate protection domain.");
    printf("Protection Domain Info:\n");
    printf("  PD Context: %p\n", (void *)ib_res.pd->context);
    printf("  PD Handle: %u\n", ib_res.pd->handle);

    /* query IB port attribute */
    ret = ibv_query_port(ib_res.ctx, IB_PORT, &ib_res.port_attr);
    check(ret == 0, "Failed to query IB port information.");
    printf("IB Port Attributes:\n");
    printf("  State: %d\n", ib_res.port_attr.state);
    printf("  Max MTU: %d\n", ib_res.port_attr.max_mtu);
    printf("  Active MTU: %d\n", ib_res.port_attr.active_mtu);
    printf("  LID: %d\n", ib_res.port_attr.lid);
    printf("  SM LID: %d\n", ib_res.port_attr.sm_lid);
    // Removed invalid field reference: capability_mask
    printf("  Port Capability Mask: %d\n", ib_res.port_attr.port_cap_flags);
    printf("  Max Message Size: %d\n", ib_res.port_attr.max_msg_sz);
    printf("  Bad PKey Counter: %d\n", ib_res.port_attr.bad_pkey_cntr);
    printf("  QKey Violations: %d\n", ib_res.port_attr.qkey_viol_cntr);
    printf("  GID Table Length: %d\n", ib_res.port_attr.gid_tbl_len);
    printf("  PKey Table Length: %d\n", ib_res.port_attr.pkey_tbl_len);
    printf("  Subnet Timeout: %d\n", ib_res.port_attr.subnet_timeout);
    printf("  Init Type Reply: %d\n", ib_res.port_attr.init_type_reply);
    printf("  Active Width: %d\n", ib_res.port_attr.active_width);
    printf("  Active Speed: %d\n", ib_res.port_attr.active_speed);
    printf("  Physical State: %d\n", ib_res.port_attr.phys_state);
    printf("  Link Layer: %d\n", ib_res.port_attr.link_layer);
    
    /* register mr */
    ib_res.ib_buf_size = config_info.msg_size * config_info.num_concurr_msgs;
    ib_res.ib_buf      = (char *) memalign (4096, ib_res.ib_buf_size);
    check (ib_res.ib_buf != NULL, "Failed to allocate ib_buf");
    printf("IB Buffer Info:\n");
    printf("  Buffer Address: %p\n", (void *)ib_res.ib_buf);
    printf("  Buffer Size: %zu\n", ib_res.ib_buf_size);

    ib_res.mr = ibv_reg_mr (ib_res.pd, (void *)ib_res.ib_buf,
			    ib_res.ib_buf_size,
			    IBV_ACCESS_LOCAL_WRITE |
			    IBV_ACCESS_REMOTE_READ |
			    IBV_ACCESS_REMOTE_WRITE);
    check (ib_res.mr != NULL, "Failed to register mr");
    printf("Memory Region Info:\n");
    printf("  MR Address: %p\n", (void *)ib_res.mr->addr);
    printf("  MR Length: %zu\n", ib_res.mr->length);
    printf("  MR LKey: %u\n", ib_res.mr->lkey);
    printf("  MR RKey: %u\n", ib_res.mr->rkey);
    
    /* query IB device attr */
    ret = ibv_query_device(ib_res.ctx, &ib_res.dev_attr);
    check(ret==0, "Failed to query device");
    printf("IB Device Attributes:\n");
    printf("  FW Version: %s\n", ib_res.dev_attr.fw_ver);
    printf("  Node GUID: %016llx\n", (unsigned long long)ib_res.dev_attr.node_guid);
    printf("  Sys Image GUID: %016llx\n", (unsigned long long)ib_res.dev_attr.sys_image_guid);
    printf("  Max MR Size: %llu\n", (unsigned long long)ib_res.dev_attr.max_mr_size);
    printf("  Page Size Cap: %llu\n", (unsigned long long)ib_res.dev_attr.page_size_cap);
    printf("  Max QP: %d\n", ib_res.dev_attr.max_qp);
    printf("  Max QP WR: %d\n", ib_res.dev_attr.max_qp_wr);
    printf("  Device Cap Flags: %llu\n", (unsigned long long)ib_res.dev_attr.device_cap_flags);
    printf("  Max SGE: %d\n", ib_res.dev_attr.max_sge);
    printf("  Max SGE RD: %d\n", ib_res.dev_attr.max_sge_rd);
    printf("  Max CQ: %d\n", ib_res.dev_attr.max_cq);
    printf("  Max CQE: %d\n", ib_res.dev_attr.max_cqe);
    printf("  Max MR: %d\n", ib_res.dev_attr.max_mr);
    printf("  Max PD: %d\n", ib_res.dev_attr.max_pd);
    printf("  Max QP RD Atom: %d\n", ib_res.dev_attr.max_qp_rd_atom);
    printf("  Max EE RD Atom: %d\n", ib_res.dev_attr.max_ee_rd_atom);
    printf("  Max RES RD Atom: %d\n", ib_res.dev_attr.max_res_rd_atom);
    printf("  Max QP Init RD Atom: %d\n", ib_res.dev_attr.max_qp_init_rd_atom);
    printf("  Max EE Init RD Atom: %d\n", ib_res.dev_attr.max_ee_init_rd_atom);
    printf("  Atomic Cap: %d\n", ib_res.dev_attr.atomic_cap);
    printf("  Max EE: %d\n", ib_res.dev_attr.max_ee);
    printf("  Max RDD: %d\n", ib_res.dev_attr.max_rdd);
    printf("  Max MW: %d\n", ib_res.dev_attr.max_mw);
    printf("  Max Raw IPv6 QP: %d\n", ib_res.dev_attr.max_raw_ipv6_qp);
    printf("  Max Raw Ethy QP: %d\n", ib_res.dev_attr.max_raw_ethy_qp);
    printf("  Max Mcast Group: %d\n", ib_res.dev_attr.max_mcast_grp);
    printf("  Max Mcast QP Attach: %d\n", ib_res.dev_attr.max_mcast_qp_attach);
    printf("  Max Total Mcast QP Attach: %d\n", ib_res.dev_attr.max_total_mcast_qp_attach);
    printf("  Max AH: %d\n", ib_res.dev_attr.max_ah);
    printf("  Max FMR: %d\n", ib_res.dev_attr.max_fmr);
    printf("  Max Map Per FMR: %d\n", ib_res.dev_attr.max_map_per_fmr);
    printf("  Max SRQ: %d\n", ib_res.dev_attr.max_srq);
    printf("  Max SRQ WR: %d\n", ib_res.dev_attr.max_srq_wr);
    printf("  Max SRQ SGE: %d\n", ib_res.dev_attr.max_srq_sge);
    printf("  Max Pkeys: %d\n", ib_res.dev_attr.max_pkeys);
    printf("  Local CA Ack Delay: %d\n", ib_res.dev_attr.local_ca_ack_delay);
    
    /* create cq */
    ib_res.cq = ibv_create_cq (ib_res.ctx, ib_res.dev_attr.max_cqe, 
			       NULL, NULL, 0);
    check (ib_res.cq != NULL, "Failed to create cq");
    printf("CQ Info:\n");
    printf("  CQ Context: %p\n", (void *)ib_res.cq->context);
    printf("  CQ Channel: %p\n", (void *)ib_res.cq->channel);
    printf("  CQ Handle: %u\n", ib_res.cq->handle);
    printf("  CQ Size: %d\n", ib_res.cq->cqe);
    
    /* create qp */
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ib_res.cq,
        .recv_cq = ib_res.cq,
        .cap = {
            .max_send_wr = 1024, //ib_res.dev_attr.max_qp_wr,
            .max_recv_wr = 1024, //ib_res.dev_attr.max_qp_wr,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };
    printf("QP Init Attributes:\n");
    printf("  send_cq: %p\n", (void *)qp_init_attr.send_cq);
    printf("  recv_cq: %p\n", (void *)qp_init_attr.recv_cq);
    printf("  cap:\n");
    printf("    max_send_wr: %d\n", qp_init_attr.cap.max_send_wr);
    printf("    max_recv_wr: %d\n", qp_init_attr.cap.max_recv_wr);
    printf("    max_send_sge: %d\n", qp_init_attr.cap.max_send_sge);
    printf("    max_recv_sge: %d\n", qp_init_attr.cap.max_recv_sge);
    printf("  qp_type: %d\n", qp_init_attr.qp_type);

    ib_res.qp = ibv_create_qp (ib_res.pd, &qp_init_attr);
    check (ib_res.qp != NULL, "Failed to create qp");
    printf("QP Info:\n");
    printf("  QP Context: %p\n", (void *)ib_res.qp->context);
    printf("  QP Handle: %u\n", ib_res.qp->qp_num);
    printf("  QP State: %d\n", ib_res.qp->state);

    /* connect QP */
    if (config_info.is_server) {
	ret = connect_qp_server ();
    } else {
	ret = connect_qp_client ();
    }
    check (ret == 0, "Failed to connect qp");

    ibv_free_device_list (dev_list);
    return 0;

 error:
    if (dev_list != NULL) {
	ibv_free_device_list (dev_list);
    }
    return -1;
}

void close_ib_connection ()
{
    if (ib_res.qp != NULL) {
	ibv_destroy_qp (ib_res.qp);
    }
 
    if (ib_res.cq != NULL) {
	ibv_destroy_cq (ib_res.cq);
    }

    if (ib_res.mr != NULL) {
	ibv_dereg_mr (ib_res.mr);
    }

    if (ib_res.pd != NULL) {
        ibv_dealloc_pd (ib_res.pd);
    }

    if (ib_res.ctx != NULL) {
        ibv_close_device (ib_res.ctx);
    }

    if (ib_res.ib_buf != NULL) {
	free (ib_res.ib_buf);
    }
}