#!/usr/bin/env python3
"""
RPC Test - Arduino App Lab 統合版 MPU側プログラム

このプログラムは Arduino App Lab 内で実行され、
システム情報をMCU側に提供します。

Arduino App Lab は main.py を自動実行するため、
このファイルを App Lab 統合版として使用します。
"""

import json
import logging
import time

from arduino.app_utils import App, Bridge
from system_monitor import SystemMonitor

# ロギング設定
logging.basicConfig(
    level=logging.INFO,
    format='[%(levelname)s] %(name)s: %(message)s'
)
logger = logging.getLogger(__name__)


UPDATE_PERIOD_SEC = 0.2


def loop() -> None:
    """Arduino App Lab のメインループ"""
    # システム情報を取得
    stats = SystemMonitor.get_all_stats()
    
    # JSON に変換
    json_data = json.dumps(stats)
    
    # MCU にデータを送信
    try:
        Bridge.call("receive_system_stats", json_data)
    except TimeoutError:
        # MCU が応答しない（一時的なハング）→ 続行
        logger.warning("Bridge call timeout - MCU may be unresponsive")
    except (ConnectionError, OSError) as e:
        # arduino-router サービス接続失敗
        logger.error("Bridge connection failed: %s", e)
    except (TypeError, ValueError) as e:
        # パラメータ形式エラー（プログラムバグ）
        logger.error("Invalid parameter to Bridge.call: %s", e)
    except Exception as e:
        # 予期しないエラー（スタックトレース含める）
        logger.error("Unexpected Bridge error: %s", e, exc_info=True)
    
    time.sleep(UPDATE_PERIOD_SEC)


# App メインループを開始
print("System Monitor - Arduino App Lab Version")
print("=" * 50)

App.run(user_loop=loop)
