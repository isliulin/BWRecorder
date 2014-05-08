/*****************************************************************************
* Model: bwgpsrecorder.qm
* File:  qmcode/qaccdetector.c
*
* This code has been generated by QM tool (see state-machine.com/qm).
* DO NOT EDIT THIS FILE MANUALLY. All your changes will be lost.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*****************************************************************************/
/* @(/3/10) ................................................................*/
#include "qp_port.h"
#include "bsp.h"
//#include "stm32_gpio.h"
#include "protocol.h"
#include "acc.h"
#include "qevents.h"
#include "trace.h"
#include "type.h"

Q_DEFINE_THIS_MODULE("qaccdetector.c")



#define DETECTING_KEPT_AMOUNT  4 //����ۼƴ�����100�Σ�����ʱ1S
#define TIMEOUT_DETECTING      (BSP_TICKS_PER_SEC >> 1) //�������

/* Active object class -----------------------------------------------------*/
/* @(/1/9) .................................................................*/
typedef struct QACCDetectorTag {
/* protected: */
    QActive super;

/* private: */
    uint8_t m_detectTimes;
    uint8_t m_state;
    QTimeEvt m_Timer;
} QACCDetector;

/* protected: */
static QState QACCDetector_initial(QACCDetector * const me, QEvt const * const e);
static QState QACCDetector_idle(QACCDetector * const me, QEvt const * const e);
static QState QACCDetector_busy(QACCDetector * const me, QEvt const * const e);



/* Local objects -----------------------------------------------------------*/
static QACCDetector l_ACCDetector; /* the single instance of the Table active object */

/* Global-scope objects ----------------------------------------------------*/
QActive * const AO_ACCDetector = &l_ACCDetector.super; /* "opaque" AO pointer */

/*..........................................................................*/
/* @(/1/32) ................................................................*/
void QACCDetector_ctor(void) {
    QACCDetector *me = &l_ACCDetector;
    QActive_ctor(&me->super, Q_STATE_CAST(&QACCDetector_initial));

    QTimeEvt_ctor(&me->m_Timer, Q_TIMEOUT_SIG);
}
/* @(/1/9) .................................................................*/
/* @(/1/9/3) ...............................................................*/
/* @(/1/9/3/0) */
static QState QACCDetector_initial(QACCDetector * const me, QEvt const * const e) {
    u8 curState;
    static const QEvt acc_on_evt = {ACC_ON_SIG, 0};
    static const QEvt acc_off_evt = {ACC_OFF_SIG, 0};

    QS_SIG_DICTIONARY(ACC_ON_SIG, (void*)0);
    QS_SIG_DICTIONARY(ACC_OFF_SIG, (void*)0);

    QS_SIG_DICTIONARY(Q_TIMEOUT_SIG, me);
    QS_SIG_DICTIONARY(ACC_ONCHANGE_SIG, me);

     //��ʼ��ʱ���ȹ㲥һ��ACC״̬
    curState = (u8)GET_ACC_STATE();
    if(0 == curState)
    {
        TRACE_(QS_USER, NULL, "[ACC] state = ACC_ON");
         QF_publish(&acc_on_evt, me);
    }
    else
    {
        TRACE_(QS_USER, NULL, "[ACC] state = ACC_OFF");
         QF_publish(&acc_off_evt, me);
    }
    return Q_TRAN(&QACCDetector_idle);
}
/* @(/1/9/3/1) .............................................................*/
static QState QACCDetector_idle(QACCDetector * const me, QEvt const * const e) {
    QState status_;
    switch (e->sig) {
        /* @(/1/9/3/1/0) */
        case ADC_ONCHANGE_SIG: {
            me->m_state = (u8)GET_ACC_STATE();
            status_ = Q_TRAN(&QACCDetector_busy);
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
/* @(/1/9/3/2) .............................................................*/
static QState QACCDetector_busy(QACCDetector * const me, QEvt const * const e) {
    QState status_;
    switch (e->sig) {
        /* @(/1/9/3/2) */
        case Q_ENTRY_SIG: {
            me->m_detectTimes = 0;

            //����1S
            QTimeEvt_postEvery(&me->m_Timer, &me->super, TIMEOUT_DETECTING);
            status_ = Q_HANDLED();
            break;
        }
        /* @(/1/9/3/2) */
        case Q_EXIT_SIG: {
            QTimeEvt_disarm(&me->m_Timer);
            status_ = Q_HANDLED();
            break;
        }
        /* @(/1/9/3/2/0) */
        case Q_TIMEOUT_SIG: {
            u8 curState = (u8)GET_ACC_STATE();
            me->m_detectTimes++;
            /* @(/1/9/3/2/0/0) */
            if (curState != me->m_state) {
                //״̬�����仯�����¿�ʼ���
                status_ = Q_TRAN(&QACCDetector_busy);
            }
            /* @(/1/9/3/2/0/1) */
            else {
                me->m_state = curState;
                /* @(/1/9/3/2/0/1/0) */
                if (me->m_detectTimes >= DETECTING_KEPT_AMOUNT) {
                    static const QEvent acc_on_Evt = {ACC_ON_SIG, 0};
                    static const QEvent acc_off_Evt = {ACC_OFF_SIG, 0};
                    TaskEvt *pe;

                    if(me->m_state == 0)
                    {
                        TRACE_(QS_USER, NULL, "[ACC] state = ACC ON");
                        SetACCState(ACC_ON);
                        //�㲥ACC������Ϣ
                        QF_publish(&acc_on_Evt, me);
                    }
                    else
                    {
                        TRACE_(QS_USER, NULL, "[ACC] state = ACC OFF");
                        SetACCState(ACC_OFF);
                        //�㲥ACC�ص���Ϣ
                        QF_publish(&acc_off_Evt, me);
                    }

                    /*
                    //�ϱ�����������״̬
                    //1.  00Ϊͣ����FEΪ���������FFΪԿ��������
                    pe = Q_NEW(TaskEvt, NEW_TASKSENDREQ_SIG);
                    pe->cmd = EVS15_CMD_START_STOP_RPT; //������
                    pe->sequence = GeneratePacketSequenceNumber();
                    pe->ret = (me->m_state == 0) ? STATE_ACCON : STATE_STOP;
                    QACTIVE_POST(AO_Gprs, (QEvt*)pe, (void*)0);
                    */
                    status_ = Q_TRAN(&QACCDetector_idle);
                }
                /* @(/1/9/3/2/0/1/1) */
                else {
                    status_ = Q_HANDLED();
                }
            }
            break;
        }
        /* @(/1/9/3/2/1) */
        case ACC_ONCHANGE_SIG: {
            //���������յ�ACC�仯�����¼��
            me->m_state = (u8)GET_ACC_STATE();
            status_ = Q_TRAN(&QACCDetector_busy);
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
