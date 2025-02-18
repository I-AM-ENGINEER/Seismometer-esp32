import numpy as np
from sklearn.linear_model import RANSACRegressor
from sklearn.linear_model import TheilSenRegressor

class TimestampFilter:
    def __init__(self, max_buffer_size=2000):
        """
        Класс для фильтрации timestamp с использованием RANSAC регрессии.

        :param max_buffer_size: Максимальный размер кольцевого буфера
        """
        self.max_buffer_size = max_buffer_size
        self.ring_buffer = []
        self.offset = 0.0
        self.slope = 0.0

    def recalc_ransac_offset_and_slope(self):
        """
        Пересчитывает параметры (offset, slope) линейной аппроксимации по 2D-точкам
        в кольцевом буфере, используя метод Theil-Sen.

        :return: (offset, slope) — интерсепт и наклон найденной прямой
        """
        if len(self.ring_buffer) < 2:
            # Недостаточно данных для регрессии — вернём по умолчанию (0, 0)
            return 0.0, 0.0

        # Преобразуем список кортежей в массивы X и y
        X = np.array([[p[0]] for p in self.ring_buffer], dtype=float)
        y = np.array([p[1] for p in self.ring_buffer], dtype=float)

        # Создаём и обучаем Theil-Sen регрессор
        theil_sen = TheilSenRegressor(random_state=42, max_iter=10)
        theil_sen.fit(X, y)

        # После обучения theil_sen — это линейная модель
        self.slope = theil_sen.coef_[0]
        self.offset = theil_sen.intercept_

        return self.offset, self.slope

    def get_approx_ts_from_sample_number(self, sample_idx):
        """
        Возвращает предсказанное значение timestamp = offset + slope * sample_idx.

        :param sample_idx: номер семпла (int)
        :return: предсказанный timestamp (float)
        """
        return self.offset + self.slope * sample_idx

    def add_to_ring_buffer(self, sample_idx, timestamp):
        """
        Добавляет пару (sample_idx, timestamp) в кольцевой буфер.
        Никакого пересчёта RANSAC внутри не делает.

        :param sample_idx: номер семпла (int)
        :param timestamp: реальное значение (float)
        """
        self.ring_buffer.append((sample_idx, timestamp))

        # Пример ограничения длины буфера
        if len(self.ring_buffer) > self.max_buffer_size:
            self.ring_buffer.pop(0)