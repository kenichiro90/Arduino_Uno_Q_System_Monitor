import psutil
import time
import logging
from typing import Dict

# ロギング設定
logger = logging.getLogger(__name__)


class SystemMonitor:
    """Linux システムモニタリング情報を収集するクラス"""
    
    # ログ出力周期制御（5秒ごと）
    _last_log_time: float = 0
    _log_interval: float = 5.0  # 秒
    
    @classmethod
    def _should_log(cls) -> bool:
        """ログ出力する時刻かチェック"""
        now = time.time()
        if now - cls._last_log_time >= cls._log_interval:
            cls._last_log_time = now
            return True
        return False
    
    @staticmethod
    def get_cpu_usage() -> float:
        """CPU使用率を取得（パーセント - ノンブロッキング）
        
        Raises:
            OSError: システムコール失敗
            PermissionError: アクセス権限不足
        """
        return psutil.cpu_percent(interval=0)
    
    @staticmethod
    def get_memory_usage() -> float:
        """メモリ使用率を取得（パーセント）
        
        Raises:
            OSError: システムコール失敗
            PermissionError: アクセス権限不足
        """
        memory = psutil.virtual_memory()
        return memory.percent
    
    @staticmethod
    def get_disk_usage(path: str = '/') -> float:
        """ディスク使用率を取得（パーセント）
        
        Args:
            path: 監視するパス（デフォルト: ルートディレクトリ）
        
        Raises:
            OSError: システムコール失敗
            PermissionError: アクセス権限不足
            FileNotFoundError: 指定パスが存在しない
        """
        disk = psutil.disk_usage(path)
        return disk.percent
    
    @classmethod
    def get_all_stats(cls) -> Dict[str, float]:
        """すべてのシステム情報を辞書で返す
        
        個別メトリクスごとに例外処理を実施。
        一部のメトリクス取得に失敗しても、他のメトリクスは続行される。
        
        Returns:
            システム情報の辞書（cpu, memory, disk, network_rx）
            取得失敗したメトリクスは 0.0
        """
        stats: Dict[str, float] = {}
        
        # CPU使用率
        try:
            stats["cpu"] = round(cls.get_cpu_usage(), 1)
        except (OSError, PermissionError) as e:
            logger.error(
                "Failed to get CPU usage: %s: %s",
                type(e).__name__, e
            )
            stats["cpu"] = 0.0
        
        # メモリ使用率
        try:
            stats["memory"] = round(cls.get_memory_usage(), 1)
        except (OSError, PermissionError) as e:
            logger.error(
                "Failed to get memory usage: %s: %s",
                type(e).__name__, e
            )
            stats["memory"] = 0.0
        
        # ディスク使用率
        try:
            stats["disk"] = round(cls.get_disk_usage(), 1)
        except (OSError, PermissionError, FileNotFoundError) as e:
            logger.error(
                "Failed to get disk usage: %s: %s",
                type(e).__name__, e
            )
            stats["disk"] = 0.0
        
        # ネットワーク（削除済み機能 - 互換性のため 0.0 固定）
        stats["network_rx"] = 0.0
        
        # ログ出力（5秒ごと）
        if cls._should_log():
            timestamp = time.strftime('%H:%M:%S')
            print(
                f"[{timestamp}] CPU: {stats['cpu']:.1f}%, "
                f"Memory: {stats['memory']:.1f}%, "
                f"Disk: {stats['disk']:.1f}%"
            )
        
        return stats
